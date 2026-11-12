//
// Implementation of Wayland Tablet/Pen event driver for FLTK.
//
// Copyright 2025-2026 by Bill Spitzak and others.
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//
// Please see the following page on how to report bugs and issues:
//
//     https://www.fltk.org/bugs.php
//

/*
 Design notes — Wayland vs. Cocoa tablet architecture
 ─────────────────────────────────────────────────────
 Cocoa delivers each tablet event as a single, complete NSEvent.
 fl_cocoa_tablet_handler() reads everything from that one object and
 dispatches immediately.

 The Wayland zwp_tablet_stable_v2 protocol splits the same information
 across a burst of per-tool callbacks (motion, pressure, tilt, down, …)
 that are bracketed by a "frame" event.  Only when frame() fires is the
 accumulated data coherent and ready to dispatch.  This file follows the
 same logical structure as the Cocoa driver but defers all dispatch to
 tool_cb_frame().

 Tool lifecycle
 ──────────────
 tool_added   → allocate TabletTool, receive type/serial/capability events
 tool_done    → capabilities are stable; finalize pen_id and trait set
 proximity_in → pen approaches a surface; note focus window
 (motion / pressure / tilt / … events accumulate)
 frame        → dispatch FLTK pen events to subscribers
 proximity_out→ pen left all surfaces; send OUT_OF_RANGE
 tool_removed → free TabletTool

 Coordinate mapping
 ──────────────────
 Wayland surface coordinates are wl_fixed_t (24.8 fixed-point) in logical
 pixels.  Dividing by the screen scale factor gives FLTK logical pixels,
 which is what Fl::Pen::e.x/y expects.  Unlike Cocoa there is no Y-flip;
 Wayland is already top-down.

 Capability / trait discovery
 ─────────────────────────────
 Cocoa requires watching the first few motion events to discover which
 axes a pen actually moves.  Wayland reports capabilities explicitly via
 capability events before tool_done(), so no discovery countdown is needed.

 Shared helper functions
 ───────────────────────
 offset_subwindow_event(), event_inside(), find_below_pen(), copy_state(),
 pen_send(), and pen_send_all() are identical in the Cocoa and Wayland
 drivers.  They are duplicated here intentionally rather than elevated to
 Fl_Base_Pen_Driver to avoid touching the shared API in this patch.
 TODO: move them to Fl_Base_Pen_Driver.cxx and expose via the header.
 */
#include "Fl_Wayland_Pen_Driver.H"
#include "src/drivers/Base/Fl_Base_Pen_Driver.H"
#include "Fl_Wayland_Screen_Driver.H"
#include "Fl_Wayland_Window_Driver.H"
#include "../../Fl_Window_Driver.H"
#include "../../../libdecor/build/fl_libdecor.h"

#include <FL/Fl.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Window.H>
#include <FL/platform.H>

#include "gtk-shell-client-protocol.h"
#include "tablet-client-protocol.h"

#include <wayland-cursor.h>
#if HAVE_CURSOR_SHAPE
#  include "cursor-shape-client-protocol.h"
#endif

extern "C" {
#include "../../../libdecor/build/fl_libdecor.h"
#include "../../../libdecor/build/fl_libdecor-plugins.h"
}

#include <iostream>
#include <cmath>
#include <cstring>
#include <cstdint>

// Physical stylus button codes from linux/input-event-codes.h.
// Defined locally so we don't depend on the kernel header directly.
#ifndef BTN_STYLUS
#  define BTN_STYLUS  0x14b   // first barrel / side button
#endif
#ifndef BTN_STYLUS2
#  define BTN_STYLUS2 0x14c   // second barrel button
#endif
#ifndef BTN_STYLUS3
#  define BTN_STYLUS3 0x149   // third barrel button (uncommon)
#endif

extern "C" {
  bool fl_is_surface_from_GTK_titlebar (struct wl_surface *surface, struct libdecor_frame *frame,
                                        bool *using_GTK);
  bool fl_is_surface_from_cairo_titlebar(struct wl_surface *surface,
                                         struct libdecor_frame *frame,
                                         bool *using_CAIRO);
}

static struct wl_surface *gtk_shell_surface;
static libdecor_frame *gtk_shell_frame = nullptr;
static Fl_Window *gtk_shell_window = nullptr;


// fl_xmousewin tracks which window last received pointer/pen events.
extern Fl_Window *fl_xmousewin;

using namespace Fl::Pen;

// C+11 Safe defined
static const State kButtonBits[] = {
    State::BUTTON0, State::BUTTON1, State::BUTTON2, State::BUTTON3
};

// ─────────────────────────────────────────────────────────────────────────────
// Per-tool state
// ─────────────────────────────────────────────────────────────────────────────

struct TabletTool {
  struct zwp_tablet_tool_v2      *wl_tool;
  enum zwp_tablet_tool_v2_type    type;

  double         decor_sx { 0 };      // surface-local x when over a decoration surface
  double         decor_sy { 0 };      // surface-local y when over a decoration surface
  struct libdecor_frame *focus_frame { nullptr }; // owning frame when over a decoration

  uint64_t                        hardware_serial;
  int                             pen_id;       // int-sized pen identity
  Trait                           capabilities; // reported by capability events
  bool                            is_new;       // true until first proximity_in
  bool                            in_proximity;
  Fl_Window                      *focus_win;    // toplevel window under the pen
  struct wl_surface              *focus_surface;
  uint32_t                        serial;
  uint32_t                        frame_time { 0 };   // ms timestamp of the frame being processed
  int                             decor_clicks { 0 }; // click count for a press on a decoration
  uint32_t                        proximity_serial { 0 };   // serial of the last proximity_in
  bool                            cursor_hidden    { false };
#if HAVE_CURSOR_SHAPE
  struct wp_cursor_shape_device_v1 *shape_device { nullptr };
#endif
  struct wl_surface *cursor_surface { nullptr };  // fallback visible cursor

  // Per-frame accumulated event data (flushed by tool_cb_frame)
  EventData  ev;
  State      prev_state;          // ev.state committed after last frame

  // Per-frame event occurrence flags (reset at end of each frame)
  bool  frame_proximity_in;
  bool  frame_proximity_out;
  bool  frame_down;               // tip touched surface this frame
  bool  frame_up;                 // tip left surface this frame
  bool  frame_motion;             // position changed this frame
  State frame_buttons_pressed;    // which BUTTON bits went active this frame
  State frame_buttons_released;   // which BUTTON bits went inactive this frame

  struct wl_list link;            // node in g_tool_list
};


// Convenience: reset all per-frame flags at the end of frame processing.
static inline void tablet_tool_reset_frame(TabletTool *t) {
  t->frame_proximity_in    = false;
  t->frame_proximity_out   = false;
  t->frame_down            = false;
  t->frame_up              = false;
  t->frame_motion          = false;
  t->frame_buttons_pressed  = (State)0;
  t->frame_buttons_released = (State)0;
}


// ─────────────────────────────────────────────────────────────────────────────
// Multi-click detection
// ─────────────────────────────────────────────────────────────────────────────

/*
 Wayland gives no click count, so we derive one from the frame timestamps.
 Follows the FLTK convention: 0 = single click, 1 = double click, 2 = triple...
 One tracker is used for the window contents (feeds Fl::e_clicks), one for the
 window decorations (titlebar double-click -> maximize/restore).
 */
struct ClickTracker {
  bool     valid  { false };
  uint32_t time   { 0 };
  double   x      { 0 }, y { 0 };
  int      pen_id { 0 };
  const void *target { nullptr };
  int      clicks { 0 };
};

static ClickTracker g_click;        // window contents
static ClickTracker g_decor_click;  // window decorations

static const uint32_t kDoubleClickMs   = 400;  // max delay between two presses
static const double   kDoubleClickDist = 10.0;  // max movement between two presses

static int click_count(ClickTracker &t, const TabletTool *tool,
                       const void *target, double x, double y) {
  const uint32_t now = tool->frame_time;
  if (t.valid && t.target == target && t.pen_id == tool->pen_id &&
      (uint32_t)(now - t.time) <= kDoubleClickMs &&
      std::fabs(x - t.x) <= kDoubleClickDist &&
      std::fabs(y - t.y) <= kDoubleClickDist) {
    t.clicks++;
  } else {
    t.clicks = 0;
  }
  t.valid  = true;
  t.time   = now;
  t.x = x;  t.y = y;
  t.pen_id = tool->pen_id;
  t.target = target;
  return t.clicks;
}


// ─────────────────────────────────────────────────────────────────────────────
// Driver-level state (one per Wayland display/seat)
// ─────────────────────────────────────────────────────────────────────────────

static struct zwp_tablet_manager_v2 *g_tablet_manager { nullptr };
static struct wl_seat               *g_wl_seat         { nullptr };
static struct zwp_tablet_seat_v2    *g_tablet_seat     { nullptr };
static struct wl_list                g_tool_list;       // list<TabletTool>
// Most-recently-active tool; used for DETECTED / CHANGED / IN_RANGE logic.
static TabletTool *g_current_tool { nullptr };
// Counter for assigning pen_ids to tools that report no hardware serial.
static int g_next_pen_id { 1 };

// ─────────────────────────────────────────────────────────────────────────────
// Wayland pen driver class
// ─────────────────────────────────────────────────────────────────────────────
namespace Fl {
namespace Pen {

class Wayland_Driver : public Driver {
public:
  Wayland_Driver() = default;
  void subscribe(Fl_Widget* widget) override;
  Trait traits()            override;
  Trait pen_traits(int id)  override;
};


void Wayland_Driver::subscribe(Fl_Widget* widget)
{
  if (!g_tablet_seat) {  // was: subscriber_list_.size() == 0
      Fl_Wayland_Screen_Driver* scr_driver =
          (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
      if (scr_driver) {
          fl_open_display();
          if (wl_seat* seat = scr_driver->get_wl_seat())
              fl_wayland_tablet_init_seat(seat);
          else
              Fl::warning("No wl_seat yet to init tablet");
      } else {
          Fl::warning("No screen driver to init tablet");
      }
  }
  Driver::subscribe(widget);
}

Trait Wayland_Driver::traits() {
  if (!g_tablet_seat) return Trait::NONE;
  // Aggregate across all known tools; a connected tablet can do at least this.
  return Trait::DRIVER_AVAILABLE | Trait::PEN_ID | Trait::ERASER |
         Trait::PRESSURE | Trait::BARREL_PRESSURE |
         Trait::TILT_X | Trait::TILT_Y | Trait::TWIST;
}

Trait Wayland_Driver::pen_traits(int pen_id) {
  if (!g_tablet_seat) return Trait::NONE;
  // pen_id == 0 means "current tool"
  TabletTool *match = nullptr;
  TabletTool *t;
  wl_list_for_each(t, &g_tool_list, link) {
    if (pen_id == 0) {
      if (t == g_current_tool) { match = t; break; }
    } else {
      if (t->pen_id == pen_id) { match = t; break; }
    }
  }
  return match ? match->capabilities : Trait::NONE;
}

Fl::Pen::Driver& newWaylandPenDriver() {
  Fl::Pen::Wayland_Driver *wayland_driver_instance = new Fl::Pen::Wayland_Driver();
  return *wayland_driver_instance;
}

} // namespace Pen
} // namespace Fl


// ─────────────────────────────────────────────────────────────────────────────
// Platform-independent helper functions
// (TODO: factor into Fl_Base_Pen_Driver.cxx, same code as Cocoa driver)
// ─────────────────────────────────────────────────────────────────────────────

/*
 Walk the widget's window ancestry and subtract each sub-window's origin from
 (x, y) so that the coordinates become widget-local.
 */
static void offset_subwindow_event(Fl_Widget *w, double &x, double &y) {
  Fl_Widget *p = w, *q;
  while (p) {
    q = p->parent();
    if (p->as_window() && q) {
      x -= p->x();
      y -= p->y();
    }
    p = q;
  }
}

/*
 Return true if (x, y) in top-window space falls inside widget w.
 */
static bool event_inside(Fl_Widget *w, double x, double y) {
  offset_subwindow_event(w, x, y);
  if (w->as_window())
    return (x >= 0 && y >= 0 && x < w->w() && y < w->h());
  return (x >= w->x() && y >= w->y() &&
          x < w->x() + w->w() && y < w->y() + w->h());
}

/*
  Find the topmost subscribed widget under (x, y) in top-window coordinates.
  Handles:
  - Normal widgets
  - Widgets inside groups
  - Subwindows (Fl_Window as child with can_expand_outside_parent_)
  - Separate top-level windows that are themselves subscribers
 */
static Fl_Widget *find_below_pen(Fl_Window *topwin, double x, double y)
{
  if (!topwin) return nullptr;

  struct Finder {
    static Fl_Widget* find_in_group(Fl_Group* g, double gx, double gy)
    {
      if (!g) return nullptr;

      // 1. Check children back-to-front (topmost first)
      for (int i = g->children() - 1; i >= 0; --i) {
        Fl_Widget* w = g->child(i);
        if (!w || !w->visible_r()) continue;

        if (!event_inside(w, gx, gy))
            continue;

        if (subscriber_list_.count(w))
            return w;

        if (Fl_Group* sg = w->as_group()) {
            if (Fl_Widget* found = find_in_group(sg, gx, gy))
                return found;
        }
      }

      // 2. Check if THIS window/group is a pen subscriber
      if (g->visible_r() && subscriber_list_.count(g))
          return g;

      return nullptr;
    }
  };

  return Finder::find_in_group(topwin, x, y);
}

/*
 Commit tool->ev into the global Fl::Pen::e and update Fl::e_x/y/root.
 The trigger is the XOR of the old and new state (bits that changed).
 */
static void copy_state(TabletTool *tool) {
  State tr = (State)((uint32_t)e.state ^ (uint32_t)tool->ev.state);
  e = tool->ev;
  e.trigger    = tr;
  Fl::e_x      = (int)tool->ev.x;
  Fl::e_y      = (int)tool->ev.y;
  Fl::e_x_root = (int)tool->ev.rx;
  Fl::e_y_root = (int)tool->ev.ry;
}

/*
 Dispatch a single pen event to widget w.
 Commits event data the first time (lazy copy), then recomputes w's local
 coordinates before calling w->handle().
 */
static int pen_send(TabletTool *tool, Fl_Widget *w, int event,
                    State trigger, bool &copied) {
  // A hidden widget (or one inside a hidden parent) never receives pen events,
  // even if it is subscribed.
  if (!w || !w->visible_r()) return 0;
  if (!copied) {
    copy_state(tool);
    copied = true;
  }
  // Recompute widget-local coordinates for this specific widget each call,
  // because different subscribers may sit at different depths.
  e.x = tool->ev.x;
  e.y = tool->ev.y;
  offset_subwindow_event(w, e.x, e.y);
  Fl::e_x    = (int)e.x;
  Fl::e_y    = (int)e.y;
  e.trigger  = trigger;
  return w->handle(event);
}

/*
 Broadcast event+trigger to every subscriber.
 */
static int pen_send_all(TabletTool *tool, int event, State trigger) {
  bool copied = false;
  for (auto &it : subscriber_list_) {
    Fl_Widget *w = it.second->widget();
    if (w) pen_send(tool, w, event, trigger, copied);
  }
  return 1;
}


// ─────────────────────────────────────────────────────────────────────────────
// Coordinate conversion (Wayland-specific)
// ─────────────────────────────────────────────────────────────────────────────

/*
 Convert a Wayland surface coordinate pair (wl_fixed_t) to FLTK logical-pixel
 coordinates and store them in tool->ev.

 Mirrors event_coords_from_surface() from Fl_Wayland_Screen_Driver.cxx but
 writes into the tool's EventData rather than the global Fl::e_x/y so that
 the data can be inspected before committing.
 */
static void coords_from_surface(TabletTool *tool,
                                wl_fixed_t sx, wl_fixed_t sy) {
  if (!tool->focus_surface) return;

  Fl_Window *win = Fl_Wayland_Window_Driver::surface_to_window(tool->focus_surface);
  if (!win) return;

  // Accumulate sub-window offsets while walking to the top-level window.
  int delta_x = 0, delta_y = 0;
  Fl_Window *w = win;
  while (w->parent()) {
    delta_x += w->x();
    delta_y += w->y();
    w = w->window();
  }
  Fl_Window *top = w; // top-level window

  float f = Fl::screen_scale(top->screen_num());

  // Apply the same menu-offset correction used by pointer_motion.
  int menu_ofs = 0;
  int *poffset = Fl_Window_Driver::menu_offset_y(win);
  if (poffset) menu_ofs = *poffset;

  tool->ev.x  = wl_fixed_to_double(sx) / f + delta_x;
  tool->ev.y  = wl_fixed_to_double(sy) / f + delta_y - menu_ofs;
  tool->ev.rx = tool->ev.x + top->x();
  tool->ev.ry = tool->ev.y + top->y();

  // Keep focus_win pointing at the top-level so the dispatch code can use it
  // as an event window without further walking.
  tool->focus_win = top;
}


// ─────────────────────────────────────────────────────────────────────────────
// zwp_tablet_tool_v2 static property callbacks
// (fire once per tool at connection time, before proximity events)
// ─────────────────────────────────────────────────────────────────────────────

static void tool_cb_hardware_serial(void *data, struct zwp_tablet_tool_v2 *,
                                    uint32_t hi, uint32_t lo) {
  static_cast<TabletTool *>(data)->hardware_serial =
      (static_cast<uint64_t>(hi) << 32) | lo;
}

static void tool_cb_done(void *data, struct zwp_tablet_tool_v2 *) {
  TabletTool *tool = static_cast<TabletTool *>(data);
  tool->capabilities |= Trait::DRIVER_AVAILABLE | Trait::PEN_ID;
  const bool eraser = (tool->type == ZWP_TABLET_TOOL_V2_TYPE_ERASER);
  if (eraser) tool->capabilities |= Trait::ERASER;

  if (tool->hardware_serial) {
    tool->pen_id = (int)(tool->hardware_serial & 0x3fffffff);
    if (eraser) tool->pen_id |= 0x40000000;   // keep pen/eraser distinct
  }
  if (tool->pen_id == 0) tool->pen_id = g_next_pen_id++;
  tool->ev.pen_id = tool->pen_id;
}

static void tool_cb_type(void *data, struct zwp_tablet_tool_v2 *,
                          uint32_t tool_type) {
  static_cast<TabletTool *>(data)->type =
    static_cast<enum zwp_tablet_tool_v2_type>(tool_type);
}


static void tool_cb_hardware_id_wacom(void * /*data*/,
                                       struct zwp_tablet_tool_v2 *,
                                       uint32_t /*id_hi*/,
                                       uint32_t /*id_lo*/) {
  // Wacom-specific product ID; not currently needed.
}

static void tool_cb_capability(void *data, struct zwp_tablet_tool_v2 *,
                                uint32_t cap) {
  TabletTool *tool = static_cast<TabletTool *>(data);
  // Map Wayland capability flags to FLTK trait bits.
  switch (cap) {
    case ZWP_TABLET_TOOL_V2_CAPABILITY_PRESSURE:
      tool->capabilities |= Trait::PRESSURE;         break;
    case ZWP_TABLET_TOOL_V2_CAPABILITY_TILT:
      tool->capabilities |= Trait::TILT_X | Trait::TILT_Y; break;
    case ZWP_TABLET_TOOL_V2_CAPABILITY_ROTATION:
      tool->capabilities |= Trait::TWIST;             break;
    case ZWP_TABLET_TOOL_V2_CAPABILITY_SLIDER:
      tool->capabilities |= Trait::BARREL_PRESSURE;   break;
    case ZWP_TABLET_TOOL_V2_CAPABILITY_DISTANCE:
      // ev.proximity is filled; no FLTK Trait::PROXIMITY yet.
      break;
    case ZWP_TABLET_TOOL_V2_CAPABILITY_WHEEL:
      // Not yet exposed through the FLTK pen API.
      break;
  }
}

static void tool_cb_removed(void *data, struct zwp_tablet_tool_v2 *wl_tool) {
  TabletTool *tool = static_cast<TabletTool *>(data);
  if (g_current_tool == tool) g_current_tool = nullptr;
#if HAVE_CURSOR_SHAPE
  if (tool->shape_device) wp_cursor_shape_device_v1_destroy(tool->shape_device);
#endif
  if (tool->cursor_surface) wl_surface_destroy(tool->cursor_surface);
  wl_list_remove(&tool->link);
  zwp_tablet_tool_v2_destroy(wl_tool);
  delete tool;
}


// ─────────────────────────────────────────────────────────────────────────────
// zwp_tablet_tool_v2 per-frame event callbacks
// (accumulate data; dispatch happens only in tool_cb_frame)
// ─────────────────────────────────────────────────────────────────────────────
// True if the app asked for no cursor on this window (cursor(FL_CURSOR_NONE))
// and has not installed a custom RGB cursor instead.
static bool tablet_wants_hidden_cursor(Fl_Window *win) {
  // The app may have called cursor() on the window under the pen (often a
  // GL/Vulkan child window), on its top-level window, or on one in between.
  for (Fl_Window *w = win; w; w = w->window()) {
    struct wld_window *xid = fl_wl_xid(w);
    if (!xid) continue;
    if (xid->custom_cursor) return false;           // custom RGB cursor wins
    if (Fl_Window_Driver::driver(w)->current_cursor() == FL_CURSOR_NONE)
      return true;
  }
  return false;
}

#if HAVE_CURSOR_SHAPE
static uint32_t tablet_shape_for(Fl_Cursor c) {
  switch (c) {
    case FL_CURSOR_CROSS:  return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR;
    case FL_CURSOR_INSERT: return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT;
    case FL_CURSOR_WAIT:   return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_WAIT;
    case FL_CURSOR_HAND:   return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRAB;
    case FL_CURSOR_HELP:   return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_HELP;
    case FL_CURSOR_MOVE:   return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE;
    default:               return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
  }
}
#endif

// Give the tool a visible cursor again after we hid it.
static void tablet_show_cursor(TabletTool *tool, Fl_Cursor c) {
  Fl_Wayland_Screen_Driver *scr = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
#if HAVE_CURSOR_SHAPE
  if (scr->wp_cursor_shape_manager) {
    if (!tool->shape_device)
      tool->shape_device = wp_cursor_shape_manager_v1_get_tablet_tool_v2(
                             scr->wp_cursor_shape_manager, tool->wl_tool);
    if (tool->shape_device) {
      wp_cursor_shape_device_v1_set_shape(tool->shape_device,
                                          tool->proximity_serial,
                                          tablet_shape_for(c));
      tool->cursor_hidden = false;
      return;
    }
  }
#endif
  // Fallback: our own cursor surface from the theme cursor.
  struct wl_cursor *wc = scr->seat ? scr->seat->default_cursor : nullptr;
  if (!wc || !wc->image_count) return;
  if (!tool->cursor_surface)
    tool->cursor_surface = wl_compositor_create_surface(scr->wl_compositor);
  struct wl_cursor_image *image = wc->images[0];
  const int scale = scr->seat->pointer_scale ? scr->seat->pointer_scale : 1;
  zwp_tablet_tool_v2_set_cursor(tool->wl_tool, tool->proximity_serial,
                                tool->cursor_surface,
                                image->hotspot_x / scale, image->hotspot_y / scale);
  wl_surface_attach(tool->cursor_surface, wl_cursor_image_get_buffer(image), 0, 0);
  wl_surface_set_buffer_scale(tool->cursor_surface, scale);
  wl_surface_damage_buffer(tool->cursor_surface, 0, 0, image->width, image->height);
  wl_surface_commit(tool->cursor_surface);
  tool->cursor_hidden = false;
}

// Make the tool's cursor match what the app wants for the window under it.
static void tablet_update_cursor(TabletTool *tool) {
  if (!tool->in_proximity || !tool->focus_surface || !tool->proximity_serial)
    return;
  Fl_Window *win = Fl_Wayland_Window_Driver::surface_to_window(tool->focus_surface);
  if (!win) return;
  if (tablet_wants_hidden_cursor(win)) {
    zwp_tablet_tool_v2_set_cursor(tool->wl_tool, tool->proximity_serial,
                                  nullptr, 0, 0);          // hide
    tool->cursor_hidden = true;
  } else if (tool->cursor_hidden) {
    // Restore the cursor of the nearest window that has a non-default one.
    Fl_Cursor c = FL_CURSOR_DEFAULT;
    for (Fl_Window *w = win; w; w = w->window()) {
      Fl_Cursor wc = Fl_Window_Driver::driver(w)->current_cursor();
      if (wc != FL_CURSOR_DEFAULT && wc != FL_CURSOR_NONE) { c = wc; break; }
    }
    tablet_show_cursor(tool, c);
  }
}

static void tool_cb_proximity_in(void *data, struct zwp_tablet_tool_v2 *,
                                  uint32_t serial,
                                  struct zwp_tablet_v2 * /*tablet*/,
                                  struct wl_surface *surface) {
  TabletTool *tool = static_cast<TabletTool *>(data);
  tool->proximity_serial   = serial;
  tool->focus_surface      = surface;
  tool->focus_frame        = nullptr;
  tool->focus_win          = Fl_Wayland_Window_Driver::surface_to_window(surface);
  tool->in_proximity       = true;
  tool->frame_proximity_in = true;
  // Set hover state immediately so ev.state is valid even without a tip-down.
  State btn_bits = tool->ev.state &
    (State::BUTTON0|State::BUTTON1|State::BUTTON2|State::BUTTON3);
  tool->ev.state = (tool->type == ZWP_TABLET_TOOL_V2_TYPE_ERASER
    ? State::ERASER_HOVERS : State::TIP_HOVERS) | btn_bits;

  if (!tool->focus_win)
  {
    // check whether surface is the headerbar of a GTK-decorated window
    bool found = false;

    auto drvr = (Fl_Wayland_Screen_Driver*)Fl::screen_driver();
    auto seat = drvr->seat;
    static bool using_GTK = seat->gtk_shell &&
                            (gtk_shell1_get_version(seat->gtk_shell) >= GTK_SURFACE1_TITLEBAR_GESTURE_SINCE_VERSION);
    // check whether surface is the headerbar of a GTK-decorated window
    if (using_GTK)
    {
        Fl_X *xp = Fl_X::first;
        while (xp && using_GTK) { // all mapped windows
            struct wld_window *xid = (struct wld_window*)xp->xid;
            if (xid->kind == Fl_Wayland_Window_Driver::DECORATED &&
                fl_is_surface_from_GTK_titlebar(surface, xid->frame,
                                                &using_GTK)) {
                gtk_shell_surface = surface;
                tool->focus_frame = xid->frame;
                gtk_shell_window = xp->w;
                found = true;
                break;
            }
            xp = xp->next;
        }
    }

    if (!found)
    {
        // check whether surface is the headerbar of a Cairo-decorated window
        static bool using_CAIRO = true;
        Fl_X *xp = Fl_X::first;
        while (xp && using_CAIRO) { // all mapped windows
            struct wld_window *xid = (struct wld_window*)xp->xid;
            if (xid->kind == Fl_Wayland_Window_Driver::DECORATED &&
                fl_is_surface_from_cairo_titlebar(surface, xid->frame,
                                                  &using_CAIRO)) {
                tool->focus_frame = xid->frame;
                found = true;
                break;
            }

            xp = xp->next;
        }
    }
    tablet_update_cursor(tool);
  }
}

static void tool_cb_proximity_out(void *data, struct zwp_tablet_tool_v2 *) {
  TabletTool *tool           = static_cast<TabletTool *>(data);
  tool->in_proximity         = false;
  tool->frame_proximity_out  = true;
  gtk_shell_window           = nullptr;
}

static void tool_cb_down(void *data, struct zwp_tablet_tool_v2 *,
                          uint32_t serial) {
  TabletTool *tool = static_cast<TabletTool *>(data);
  tool->serial = serial;
  // Tip contact; preserve any side-button bits already present.
  State btn_bits = tool->ev.state &
    (State::BUTTON0|State::BUTTON1|State::BUTTON2|State::BUTTON3);
  tool->ev.state = (tool->type == ZWP_TABLET_TOOL_V2_TYPE_ERASER
    ? State::ERASER_DOWN : State::TIP_DOWN) | btn_bits;
  tool->frame_down = true;
}

static void tool_cb_up(void *data, struct zwp_tablet_tool_v2 *) {
  TabletTool *tool = static_cast<TabletTool *>(data);
  State btn_bits = tool->ev.state &
    (State::BUTTON0|State::BUTTON1|State::BUTTON2|State::BUTTON3);
  tool->ev.state = (tool->type == ZWP_TABLET_TOOL_V2_TYPE_ERASER
    ? State::ERASER_HOVERS : State::TIP_HOVERS) | btn_bits;
  tool->frame_up = true;
}

static void tool_cb_motion(void *data, struct zwp_tablet_tool_v2 *,
                            wl_fixed_t x, wl_fixed_t y) {
  TabletTool *tool = static_cast<TabletTool *>(data);
  tool->decor_sx = wl_fixed_to_double(x);
  tool->decor_sy = wl_fixed_to_double(y);
  coords_from_surface(tool, x, y);
  tool->frame_motion = true;
}

static void tool_cb_pressure(void *data, struct zwp_tablet_tool_v2 *,
                              uint32_t pressure) {
  // Wayland: [0, 65535] → FLTK: [0.0, 1.0]
  static_cast<TabletTool *>(data)->ev.pressure = pressure / 65535.0;
}

static void tool_cb_distance(void *data, struct zwp_tablet_tool_v2 *,
                              uint32_t distance) {
  // Wayland: [0, 65535] → FLTK: [0.0, 1.0]
  static_cast<TabletTool *>(data)->ev.proximity = distance / 65535.0;
}

static void tool_cb_tilt(void *data, struct zwp_tablet_tool_v2 *,
                          wl_fixed_t tilt_x, wl_fixed_t tilt_y) {
  TabletTool *tool = static_cast<TabletTool *>(data);
  // Wayland reports degrees in [-90, 90]; normalise to [-1, 1] to match
  // NSEvent's tilt range.  tilt_x is negated to match the Cocoa driver's
  // convention (ev.tilt_x = -tilt.x).
  tool->ev.tilt_x = -wl_fixed_to_double(tilt_x) / 90.0;
  tool->ev.tilt_y = -wl_fixed_to_double(tilt_y) / 90.0;
}

static void tool_cb_rotation(void *data, struct zwp_tablet_tool_v2 *,
                              wl_fixed_t degrees) {
  // Wayland: clockwise rotation in [0, 360).
  // Cocoa returns the same unit (degrees), so store as-is.
  static_cast<TabletTool *>(data)->ev.twist = wl_fixed_to_double(degrees);
}

static void tool_cb_slider(void *data, struct zwp_tablet_tool_v2 *,
                            int32_t position) {
  // Wayland: [-65535, 65535] → FLTK: [-1.0, 1.0]
  // Maps to barrel_pressure, which in Cocoa is tangentialPressure.
  static_cast<TabletTool *>(data)->ev.barrel_pressure = position / 65535.0;
}

static void tool_cb_wheel(void * /*data*/, struct zwp_tablet_tool_v2 *,
                           wl_fixed_t /*degrees*/, int32_t /*clicks*/) {
  // Tablet wheel / lens events; not yet exposed via the FLTK pen API.
}

static void tool_cb_button(void *data, struct zwp_tablet_tool_v2 *,
                            uint32_t /*serial*/, uint32_t button,
                            uint32_t button_state) {
  TabletTool *tool = static_cast<TabletTool *>(data);
  bool pressed = (button_state == ZWP_TABLET_TOOL_V2_BUTTON_STATE_PRESSED);

  // Map physical button codes to State bits.
  State bit = (State)0;
  switch (button) {
    case BTN_STYLUS:  bit = State::BUTTON1; break; // upper barell button
    case BTN_STYLUS2: bit = State::BUTTON0; break; // lower barrel button, closer to the pen tip
    case BTN_STYLUS3: bit = State::BUTTON2; break;
    default: break;
  }
  if ((uint32_t)bit == 0) return;

  if (pressed) {
    tool->ev.state              |= bit;
    tool->frame_buttons_pressed |= bit;
  } else {
    int state = static_cast<int>(tool->ev.state) & ~(static_cast<int>(bit));
    tool->ev.state = static_cast<Fl::Pen::State>(state);
    tool->frame_buttons_released |= bit;
  }
}

#include "../../../libdecor/src/libdecor.h"
#include "xdg-shell-client-protocol.h"
#include "gtk-shell-client-protocol.h"

/**
 * Generic function to handle titlebar events on each backend.  Sadly,
 * libdecor does not expose where are each button, so we rely on offsets
 * from the right of the titlebar.
 *
 * @param win             Fl_Window
 * @param x_from_right    x position to the right of window
 * @param kCloseBtn       min position of the close button from right
 * @param kMaximizeBtn    min position of the maximize button from right
 * @param kMinimizeBtn    min position of the minimize button from right
 */
static int handle_titlebar_events(Fl_Window* win, TabletTool* tool,
                                  double x_from_right,
                                  double kCloseBtn, double kMaximizeBtn,
                                  double kMinimizeBtn)
{
    int out = 0;
    libdecor_frame_ref(tool->focus_frame);
    if (x_from_right < kCloseBtn) {
        // Close button
        //win->hide(); <-- do not use this.  Crashes on Vulkan
        libdecor_frame_close(tool->focus_frame);
        out = 1;
    } else if (x_from_right < kMaximizeBtn) {
        // Maximize / restore
        if (win->maximize_active())
            win->un_maximize();
        else
            win->maximize();
        out = 1;
    } else if (x_from_right < kMinimizeBtn) {
        // Minimize (works)
        win->iconize();
        out = 1;
    } else if (tool->decor_clicks == 1 && win->resizable()) {
        // Double-click on the title bar → maximize / restore
        if (win->maximize_active())
            win->un_maximize();
        else
            win->maximize();
        g_decor_click.valid = false;   // a third tap starts a new sequence
        out = 1;
    } else if (tool->serial) {
        // Title bar drag → move (works)
        libdecor_frame_move(tool->focus_frame, g_wl_seat,
                            tool->serial);
        out = 1;
    }
    libdecor_frame_unref(tool->focus_frame);
    return out;
}

/**
 * Convert TabletTool evens to libdecor's cairo plug-in.
 *
 * @param w     Fl_Window
 * @param tool  TabletTool state
 */
static int handle_cairo_events(Fl_Window* win, TabletTool* tool)
{
    if (win) win->show();

    double sx = tool->decor_sx;
    double sy = tool->decor_sy;

    int left = 0, top = 0;
    libdecor_frame_translate_coordinate(tool->focus_frame, 0, 0,
                                        &left, &top);

    float f  = Fl::screen_scale(win->screen_num());
    // Total decoration surface dimensions in surface-local pixels:
    double total_decor_w = win->w() * f + 2 * left;
    double total_decor_h = win->h() * f + top;  // bottom border ≈ left

    /**
     * Try handling title bar first.  Cairo always exposes all buttons,
     * regardless of window's capabilities.
     *
     */
    if(sy >= 0 && sy < top) {
        double kBtnW   = 24.0;
        double kBtnPad =  4.0;
        double kRPad   =  6.0;
        double x_from_right = total_decor_w - tool->decor_sx;

        double kCloseBtn = kRPad + kBtnW;
        double kMaximizeBtn = kCloseBtn + kBtnPad + kBtnW;
        double kMinimizeBtn = kMaximizeBtn + kBtnPad + kBtnW;
        int ret = handle_titlebar_events(win, tool, x_from_right,
                                         kCloseBtn, kMaximizeBtn,
                                         kMinimizeBtn);
        if (ret)
            return ret;
    }

    return 0;
}

/*
  Convert pen events over the titlebar or resize area into libdecor actions.

  \return a value that indicate how the caller shall continue processing the event.
  The return values are yet to be defined.
*/
static int handle_gtk_frame_events(Fl_Window* win, TabletTool *tool) {
    if (win) win->show();

    double sx = tool->decor_sx;
    double sy = tool->decor_sy;

    float f  = Fl::screen_scale(win->screen_num());
    int left = 0, top = 0;
    libdecor_frame_translate_coordinate(tool->focus_frame, 0, 0,
                                        &left, &top);

    // Total decoration surface dimensions in surface-local pixels:
    double total_decor_w = win->w() * f + 2 * left;
    double total_decor_h = win->h() * f + top;  // bottom border ≈ left

    /**
     * Try handling title bar first.  GTK3 may hide some buttons based
     * on Window capabilities.
     *
     */
    if(sy >= 0 && sy < top) {
        double kBtnW   = 24.0;
        double kBtnPad =  4.0;
        double kRPad   =  6.0;
        double x_from_right = total_decor_w - tool->decor_sx;

        double kCloseBtn = kRPad + kBtnW;
        double kMaximizeBtn = kCloseBtn + kBtnPad + kBtnW;
        double kMinimizeBtn = kMaximizeBtn + kBtnPad + kBtnW;
        if (!win->resizable())
        {
            kMinimizeBtn = kMaximizeBtn;
            kMaximizeBtn = -10000;
        }
        int ret = handle_titlebar_events(win, tool, x_from_right,
                                         kCloseBtn, kMaximizeBtn,
                                         kMinimizeBtn);
        if (ret)
            return ret;
    }
    return 0;
}

/*
 tool_cb_frame — the main dispatch point.

 All per-frame data has been accumulated by the callbacks above.  This
 function mirrors the body of fl_cocoa_tablet_handler() but reads from
 tool->ev rather than an NSEvent.

 Ordering within one frame:
   1. proximity-out  → OUT_OF_RANGE broadcast, below_pen cleanup, early return
   2. proximity-in   → DETECTED / CHANGED / IN_RANGE broadcast
   3. modal/grab guard
   4. below_pen ENTER/LEAVE tracking
   5. tip down       → TOUCH
   6. tip up         → LIFT
   7. barrel buttons → BUTTON_PUSH / BUTTON_RELEASE
   8. motion         → DRAW (if pushed) or HOVER
   9. reset per-frame flags
 */
static void tool_cb_frame(void *data, struct zwp_tablet_tool_v2 *,
                           uint32_t time) {
  TabletTool *tool = static_cast<TabletTool *>(data);
  tool->frame_time = time;

  // ── 0. Snapshot and clear the per-frame flags ────────────────────────────
  // Must happen before any dispatch: see the comment at the top.
  struct FrameFlags {
    bool  prox_in;
    bool  prox_out;
    bool  down;
    bool  up;
    bool  motion;
    State btn_pressed;
    State btn_released;
  } fr;
  fr.prox_in      = tool->frame_proximity_in;
  fr.prox_out     = tool->frame_proximity_out;
  fr.down         = tool->frame_down;
  fr.up           = tool->frame_up;
  fr.motion       = tool->frame_motion;
  fr.btn_pressed  = tool->frame_buttons_pressed;
  fr.btn_released = tool->frame_buttons_released;
  tablet_tool_reset_frame(tool);

  // ── 1. Proximity-out ──────────────────────────────────────────────────────
  if (fr.prox_out) {
    pen_send_all(tool, Fl::Pen::OUT_OF_RANGE, (State)0);
    // Clean up below_pen_ if this tool owns it.
    if (below_pen_ && below_pen_->widget()) {
      bool copied = false;
      pen_send(tool, below_pen_->widget(),
               Fl::Pen::LEAVE, (State)0, copied);
    }
    below_pen_ = nullptr;

    Fl::belowmouse(nullptr);
    Fl_Tooltip::enter(nullptr);

    if (pushed_) {
      Fl::pushed(nullptr);
      pushed_ = nullptr;
    }
    tool->focus_surface = nullptr;
    tool->focus_win     = nullptr;
    tool->prev_state = tool->ev.state;
    return;
  }

  // ── 2. Proximity-in notification ─────────────────────────────────────────
  if (fr.prox_in) {
    tool->ev.pen_id = tool->pen_id;
    if (tool->is_new) {
      tool->is_new = false;
      pen_send_all(tool, Fl::Pen::DETECTED, (State)0);
    } else if (g_current_tool && g_current_tool != tool) {
      pen_send_all(tool, Fl::Pen::CHANGED, (State)0);
    } else {
      pen_send_all(tool, Fl::Pen::IN_RANGE, (State)0);
    }
    g_current_tool = tool;
  }

  // No subscribers or no focus window → nothing more to do.
  // Check if over decoration and if so, do an action if needed.
  if (!tool->focus_win || subscriber_list_.empty()) {

      // ── frame down on decoration ───────────────────────────────────────
      if (tool->focus_frame && fr.down) {
          tool->decor_clicks = click_count(g_decor_click, tool,
                                           tool->focus_frame,
                                           tool->decor_sx, tool->decor_sy);

          // Find the Fl_Window that owns this frame.
          Fl_Window* win = nullptr;
          for (win = Fl::first_window(); win;
               win = Fl::next_window(win)) {
              wld_window* xid = fl_wl_xid(win);
              if (xid->kind != Fl_Wayland_Window_Driver::DECORATED ||
                  !xid->frame)
                  continue;
              if (xid->frame == tool->focus_frame)
                  break;
          }

          if (win) {
              enum plugin_kind plugin = get_plugin_kind(tool->focus_frame);

              int result = 0;

              switch(plugin)
              {
              case SSD:
                  // Nothing to do.
                  break;
              case GTK3:
                  result = handle_gtk_frame_events(win, tool);
                  break;
              case CAIRO:
              case UNKNOWN:
                  // Handle titlebar for Cairo and any unknown plugin
                  result = handle_cairo_events(win, tool);
                  break;
              }
              (void)result;
          } // if (win)
      } // focus_frame && fr.down

      tool->prev_state = tool->ev.state;
      return;
  }

  Fl_Window *eventWindow = nullptr;
  if (Fl::grab()) {
    eventWindow = Fl::grab();
  } else if (Fl::modal()) {
    eventWindow = Fl::modal();
  } else {
    eventWindow = tool->focus_win;
  }

  // Safe-guard
  if (!eventWindow)
  {
    tool->prev_state = tool->ev.state;
    return;
  }

  bool is_menu_window = eventWindow->menu_window();

  if (!is_menu_window) {
      // ── 3. Modal / grab guards ──────────────────────────────────────────
      if (Fl::grab() && Fl::grab() != eventWindow) {
          tool->prev_state = tool->ev.state;
          return;
      }
      if (Fl::modal() && Fl::modal() != eventWindow) {
          tool->prev_state = tool->ev.state;
          return;
      }
  }

  fl_xmousewin = eventWindow;

  bool event_data_copied = false;
  Fl_Widget *receiver    = nullptr;
  bool       is_pushed   = false;

  // Mouse fallback: emulate the equivalent standard FLTK mouse event.
  // NOTE: Fl::handle(FL_PUSH) here may enter a nested event loop (menus).
  // That is safe now because the per-frame flags were already cleared above
  // and everything below works from the `fr` snapshot.
  auto mouse_fallback = [&](bool frame_down, bool frame_up, bool frame_motion) {
    Fl::e_x      = (int)tool->ev.x;
    Fl::e_y      = (int)tool->ev.y;
    Fl::e_x_root = (int)tool->ev.rx;
    Fl::e_y_root = (int)tool->ev.ry;
    if (frame_down) {
      Fl::e_state  |= FL_BUTTON1;
      Fl::e_keysym  = FL_Button + 1;
      Fl::e_is_click = 1;
      Fl::handle(FL_PUSH, eventWindow);
    } else if (frame_up) {
      Fl::e_state  &= ~FL_BUTTON1;
      Fl::e_keysym  = FL_Button + 1;
      Fl::handle(FL_RELEASE, eventWindow);
    } else if (frame_motion) {
      Fl::handle(Fl::pushed() ? FL_DRAG : FL_MOVE, eventWindow);
    }
  };

  // ── 4. Receiver selection & below_pen ENTER/LEAVE ────────────────────────
  // If the widget that grabbed the pen was hidden meanwhile, release it.
  if (pushed_ && pushed_->widget() && !pushed_->widget()->visible_r()) {
    if (Fl::pushed() == pushed_->widget()) Fl::pushed(nullptr);
    pushed_ = nullptr;
  }
  if (pushed_ && pushed_->widget() && Fl::pushed() == pushed_->widget()) {
    // An earlier tip-down fixed this tool's receiver until the tip lifts.
    receiver  = pushed_->widget();
    is_pushed = true;
  } else {
    auto bpen_widget = below_pen_ ? below_pen_->widget() : nullptr;
    auto bpen_old    = (Fl::belowmouse() == bpen_widget) ? bpen_widget : nullptr;
    auto bpen_now    = find_below_pen(eventWindow, tool->ev.x, tool->ev.y);

    // Prevent flickering by ignoring the tooltip window entirely ──
    if (bpen_now) {
      Fl_Window *win = bpen_now->as_window() ? bpen_now->as_window() : bpen_now->window();
      if (win && win->tooltip_window()) {
        bpen_now = bpen_old; // Pretend the pen never left the underlying widget
      }
    }

    // ── Widget Transition Logic ────────────────────────────────────────────
    if (bpen_now != bpen_old) {
      if (bpen_old) {
        pen_send(tool, bpen_old, Fl::Pen::LEAVE, (State)0,
                 event_data_copied);
      }
      below_pen_ = nullptr;
      if (bpen_now) {
        State hover_state = (tool->type == ZWP_TABLET_TOOL_V2_TYPE_ERASER)
          ? State::ERASER_HOVERS : State::TIP_HOVERS;
        if (pen_send(tool, bpen_now, Fl::Pen::ENTER, hover_state,
                     event_data_copied)) {
          below_pen_ = subscriber_list_[bpen_now];
        }

        // Update standard FLTK hover state regardless of pen_send success.
        // This ensures passive widgets (like Fl_Box) still show their standard tooltips!
        Fl::belowmouse(bpen_now);
        Fl_Tooltip::enter(bpen_now);
      } else {
        // Moving into empty space
        Fl::belowmouse(nullptr);
        Fl_Tooltip::enter(nullptr);
      }
    }
    receiver = below_pen_ ? below_pen_->widget() : nullptr;
  }

  if (!receiver) {
    mouse_fallback(fr.down, fr.up, fr.motion);
    tool->prev_state = tool->ev.state;
    return;
  }

  bool pen_handled = false;

  // ── 5. Tip down → TOUCH ──────────────────────────────────────────────────
  if (fr.down) {
    if (!is_pushed) {
      pushed_ = subscriber_list_[receiver];
      Fl::pushed(receiver);
    }
    Fl::e_is_click = 1;
    Fl::Private::e_x_down = (int)tool->ev.x;
    Fl::Private::e_y_down = (int)tool->ev.y;
    Fl::e_clicks = click_count(g_click, tool, tool->focus_win,
                               tool->ev.x, tool->ev.y);
    pen_handled |= pen_send(tool, receiver, Fl::Pen::TOUCH,
                            tool->ev.state & (State::TIP_DOWN|State::ERASER_DOWN),
                            event_data_copied);
    if (!pen_handled)
      mouse_fallback(true, false, false);
  }

  // ── 6. Tip up → LIFT ─────────────────────────────────────────────────────
  if (fr.up) {
    if ((tool->ev.state & State::ANY_DOWN) == (State)0) {
      Fl::pushed(nullptr);
      pushed_ = nullptr;
    }
    State trigger = (tool->type == ZWP_TABLET_TOOL_V2_TYPE_ERASER)
      ? State::ERASER_HOVERS : State::TIP_HOVERS;
    bool handled = pen_send(tool, receiver, Fl::Pen::LIFT, trigger,
                            event_data_copied);
    pen_handled |= handled;
    if (!handled)
      mouse_fallback(false, true, false);
  }

  // ── 7. Barrel button events ───────────────────────────────────────────────
  for (State bit : kButtonBits) {
    if ((uint32_t)(fr.btn_pressed) & (uint32_t)bit)
      pen_handled |= pen_send(tool, receiver, Fl::Pen::BUTTON_PUSH, bit,
                              event_data_copied);
    if ((uint32_t)(fr.btn_released) & (uint32_t)bit)
      pen_handled |= pen_send(tool, receiver, Fl::Pen::BUTTON_RELEASE, bit,
                              event_data_copied);
    // No mouse fallback for barrel buttons — there's no standard mouse
    // equivalent, and the widget already ignored the pen event.
  }

  // ── 8. Motion → DRAW or HOVER ────────────────────────────────────────────
  if (fr.motion) {
    if (Fl::e_is_click &&
        (std::fabs(tool->ev.x - Fl::Private::e_x_down) > 5.0 ||
         std::fabs(tool->ev.y - Fl::Private::e_y_down) > 5.0))
      Fl::e_is_click = 0;

    bool handled = pen_send(tool, receiver,
                            is_pushed ? Fl::Pen::DRAW : Fl::Pen::HOVER,
                            (State)0, event_data_copied);
    pen_handled |= handled;
    if (!handled)
      mouse_fallback(false, false, true);
  }

  tool->prev_state = tool->ev.state;
}

// ─────────────────────────────────────────────────────────────────────────────
// zwp_tablet_tool_v2 listener
// ─────────────────────────────────────────────────────────────────────────────

static const struct zwp_tablet_tool_v2_listener tablet_tool_listener = {
  .type               = tool_cb_type,
  .hardware_serial    = tool_cb_hardware_serial,
  .hardware_id_wacom  = tool_cb_hardware_id_wacom,
  .capability         = tool_cb_capability,
  .done               = tool_cb_done,
  .removed            = tool_cb_removed,
  .proximity_in       = tool_cb_proximity_in,
  .proximity_out      = tool_cb_proximity_out,
  .down               = tool_cb_down,
  .up                 = tool_cb_up,
  .motion             = tool_cb_motion,
  .pressure           = tool_cb_pressure,
  .distance           = tool_cb_distance,
  .tilt               = tool_cb_tilt,
  .rotation           = tool_cb_rotation,
  .slider             = tool_cb_slider,
  .wheel              = tool_cb_wheel,
  .button             = tool_cb_button,
  .frame              = tool_cb_frame,
};


// ─────────────────────────────────────────────────────────────────────────────
// zwp_tablet_v2 callbacks (physical tablet device object)
// ─────────────────────────────────────────────────────────────────────────────

// We do not need per-tablet state at the moment; the tablet object is created
// and destroyed but its properties (name, VID/PID, device path) are unused.

static void tablet_cb_name(void * /*data*/, struct zwp_tablet_v2 *,
                           const char * /*name*/) {}
static void tablet_cb_id(void * /*data*/, struct zwp_tablet_v2 *,
                          uint32_t /*vid*/, uint32_t /*pid*/) {}
static void tablet_cb_path(void * /*data*/, struct zwp_tablet_v2 *,
                            const char * /*path*/) {}
static void tablet_cb_done(void * /*data*/, struct zwp_tablet_v2 *) {}
static void tablet_cb_removed(void *data, struct zwp_tablet_v2 *wl_tablet) {
  // data was set to the zwp_tablet_v2* itself for easy cleanup
  zwp_tablet_v2_destroy(wl_tablet);
}

static const struct zwp_tablet_v2_listener tablet_listener = {
  .name    = tablet_cb_name,
  .id      = tablet_cb_id,
  .path    = tablet_cb_path,
  .done    = tablet_cb_done,
  .removed = tablet_cb_removed,
};


// ─────────────────────────────────────────────────────────────────────────────
// zwp_tablet_seat_v2 callbacks
// ─────────────────────────────────────────────────────────────────────────────

static void tablet_seat_cb_tablet_added(void * /*data*/,
                                         struct zwp_tablet_seat_v2 *,
                                         struct zwp_tablet_v2 *wl_tablet) {
  // Use the tablet object pointer itself as callback data; we only need it
  // for cleanup in tablet_cb_removed.
  zwp_tablet_v2_add_listener(wl_tablet, &tablet_listener, wl_tablet);
}

static void tablet_seat_cb_tool_added(void * /*data*/,
                                       struct zwp_tablet_seat_v2 *,
                                       struct zwp_tablet_tool_v2 *wl_tool) {
  TabletTool *tool = new TabletTool();
  tool->wl_tool       = wl_tool;
  tool->type          = ZWP_TABLET_TOOL_V2_TYPE_PEN;
  tool->hardware_serial = 0;
  tool->pen_id        = 0;  // set in tool_cb_hardware_serial or tool_cb_done
  tool->capabilities  = Trait::NONE;
  tool->is_new        = true;
  tool->in_proximity  = false;
  tool->focus_win     = nullptr;
  tool->focus_surface = nullptr;
  tool->ev            = EventData();  // value-initialised defaults from struct
  tool->prev_state    = (State)0;
  tablet_tool_reset_frame(tool);
  wl_list_insert(&g_tool_list, &tool->link);
  zwp_tablet_tool_v2_add_listener(wl_tool, &tablet_tool_listener, tool);
}

static void tablet_seat_cb_pad_added(void * /*data*/,
                                      struct zwp_tablet_seat_v2 *,
                                      struct zwp_tablet_pad_v2 *wl_pad) {
  // Tablet pad (touch ring, strip, buttons) — not yet mapped to FLTK pen API.
  // Destroy immediately to avoid resource leak.
  zwp_tablet_pad_v2_destroy(wl_pad);
}

static const struct zwp_tablet_seat_v2_listener tablet_seat_listener = {
  .tablet_added = tablet_seat_cb_tablet_added,
  .tool_added   = tablet_seat_cb_tool_added,
  .pad_added    = tablet_seat_cb_pad_added,
};


// ─────────────────────────────────────────────────────────────────────────────
// Public API called from Fl_Wayland_Screen_Driver.cxx
// ─────────────────────────────────────────────────────────────────────────────

/*
 Try to create the tablet seat once we have both the manager and the wl_seat.
 Called from both fl_wayland_tablet_set_manager() and
 fl_wayland_tablet_init_seat() to handle arbitrary ordering.
 */
static void tablet_try_init() {
  if (!g_tablet_manager || !g_wl_seat || g_tablet_seat) return;
  wl_list_init(&g_tool_list);
  g_tablet_seat = zwp_tablet_manager_v2_get_tablet_seat(
    g_tablet_manager, g_wl_seat);
  if (g_tablet_seat)
    zwp_tablet_seat_v2_add_listener(g_tablet_seat,
                                    &tablet_seat_listener, nullptr);
}

void fl_wayland_tablet_set_manager(struct zwp_tablet_manager_v2 *manager) {
  g_tablet_manager = manager;
  tablet_try_init(); // no-op if seat not yet available; seat-first path in subscribe()
}

void fl_wayland_tablet_init_seat(struct wl_seat *wl_seat) {
  if (!wl_seat || g_wl_seat) return;  // guard against null and double-init
  g_wl_seat = wl_seat;
  tablet_try_init();
}

void fl_wayland_tablet_cleanup() {
  if (g_tablet_seat) {
    // Destroy all tool objects first.
    TabletTool *t, *tmp;
    wl_list_for_each_safe(t, tmp, &g_tool_list, link) {
#if HAVE_CURSOR_SHAPE
      if (t->shape_device) wp_cursor_shape_device_v1_destroy(t->shape_device);
#endif
      if (t->cursor_surface) wl_surface_destroy(t->cursor_surface);
      wl_list_remove(&t->link);
      zwp_tablet_tool_v2_destroy(t->wl_tool);
      delete t;
    }
    zwp_tablet_seat_v2_destroy(g_tablet_seat);
    g_tablet_seat = nullptr;
  }
  if (g_tablet_manager) {
    zwp_tablet_manager_v2_destroy(g_tablet_manager);
    g_tablet_manager = nullptr;
  }
  g_wl_seat      = nullptr;
  g_current_tool = nullptr;
  below_pen_     = nullptr;
  pushed_        = nullptr;
}

// Called when a window's cursor changes, so a tool already in proximity
// reacts without having to leave and re-enter.
void fl_wayland_tablet_cursor_changed(Fl_Window *win) {
  if (!g_tablet_seat || !win) return;
  TabletTool *t;
  wl_list_for_each(t, &g_tool_list, link) {
    if (!t->in_proximity || !t->focus_surface) continue;
    Fl_Window *fw = Fl_Wayland_Window_Driver::surface_to_window(t->focus_surface);
    if (fw && fw->top_window() == win->top_window()) tablet_update_cursor(t);
  }
}

/*
 Called from Fl_Wayland_Window_Driver::hide() just before a wl_surface is
 destroyed. Any tablet tool currently focused on that surface gets its
 cached surface/window pointers cleared, and any below_pen_/pushed_
 widget belonging to the window being closed is released, so a frame
 event already queued by the compositor can't dereference freed memory.
 */
void fl_wayland_tablet_surface_destroyed(struct wl_surface *surface) {
  if (!g_tablet_seat || !surface) return;

  TabletTool *t;
  wl_list_for_each(t, &g_tool_list, link) {
    if (t->focus_surface != surface) continue;

    Fl_Window *closing = t->focus_win;

    if (below_pen_ && below_pen_->widget() &&
        below_pen_->widget()->top_window() == closing) {
      bool copied = false;
      pen_send(t, below_pen_->widget(), Fl::Pen::LEAVE, (State)0, copied);
      below_pen_ = nullptr;
      Fl::belowmouse(nullptr);
    }
    if (pushed_ && pushed_->widget() &&
        pushed_->widget()->top_window() == closing) {
      Fl::pushed(nullptr);
      pushed_ = nullptr;
    }

    t->focus_surface = nullptr;
    t->focus_win     = nullptr;
    t->in_proximity  = false;
    tablet_tool_reset_frame(t);
  }
}
