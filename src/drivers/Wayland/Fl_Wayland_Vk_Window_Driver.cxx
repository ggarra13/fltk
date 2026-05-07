//
// Class Fl_Wayland_Vk_Window_Driver for the Fast Light Tool Kit (FLTK).
//
// Copyright 2021-2022 by Bill Spitzak and others.
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

#include <config.h>
#if HAVE_VK
#include <stdexcept>
#include <dlfcn.h>

#include <FL/platform.H>
#include "../../Fl_Screen_Driver.H"

#define VK_USE_PLATFORM_WAYLAND_KHR
#include <vulkan/vulkan.h>

#include "Fl_Wayland_Vk_Window_Driver.H"
#ifdef FLTK_USE_X11
#include "../X11/Fl_X11_Vk_Window_Driver.H"
#endif
#include "../../Fl_Vk_Choice.H"
#include "../../../libdecor/build/fl_libdecor.h"
#include "Fl_Wayland_Window_Driver.H"

// Describes crap needed to create a VKContext.
class Fl_Wayland_Vk_Choice : public Fl_Vk_Choice {
  friend class Fl_Wayland_Vk_Window_Driver;

private:
  int pixelformat; // the visual to use
public:
  Fl_Wayland_Vk_Choice(int m, const int *alistp, Fl_Vk_Choice *n)
    : Fl_Vk_Choice(m, alistp, n) {
    pixelformat = 0;
  }
};

Fl_Vk_Window_Driver *Fl_Vk_Window_Driver::newVkWindowDriver(Fl_Vk_Window *w) {
#ifdef FLTK_USE_X11
  if (!Fl_Wayland_Screen_Driver::wl_display)
    return new Fl_X11_Vk_Window_Driver(w);
#endif
  return new Fl_Wayland_Vk_Window_Driver(w);
}

// Wayland's new methodology requires explicit sync
int Fl_Wayland_Vk_Window_Driver::explicit_sync = -1;

Fl_Wayland_Vk_Window_Driver::Fl_Wayland_Vk_Window_Driver(Fl_Vk_Window *win)
    : Fl_Vk_Window_Driver(win)
    , m_current_wld_scale(0)   // 0 = not yet initialised; updated on first resize()
{
}

int Fl_Wayland_Vk_Window_Driver::genlistsize() {
#if USE_XFT || FLTK_USE_CAIRO
  return 256;
#else
  return 0x10000;
#endif
}

void Fl_Wayland_Vk_Window_Driver::get_list(Fl_Font_Descriptor *fd, int r) {
#if USE_XFT || FLTK_USE_CAIRO
  /* We hope not to come here: We hope that any system using XFT will also
   * have sufficient Vulkan capability to support our font texture pile
   * mechansim, allowing XFT to render the face directly. */
  (void)fd;
  (void)r;
#else
  // @todo:
  // Fl_Wayland_Font_Descriptor *gl_fd = (Fl_Wayland_Font_Descriptor*)fd;
  // if (gl_fd->vkok[r]) return;
  // gl_fd->vkok[r] = 1;
  // unsigned int ii = r * 0x400;
  // for (int i = 0; i < 0x400; i++) {
  //   XFontStruct *font = NULL;
  //   unsigned short id;
  //   fl_XGetUtf8FontAndGlyph(gl_fd->font, ii, &font, &id);
  //   // if (font) glXUseXFont(font->fid, id, 1, gl_fd->listbase+ii);
  //   ii++;
  // }
#endif
}

#if !(USE_XFT || FLTK_USE_CAIRO)
Fl_Font_Descriptor **Fl_Wayland_Vk_Window_Driver::fontnum_to_fontdescriptor(int fnum) {
  Fl_Wayland_Fontdesc *s = ((Fl_Wayland_Fontdesc *)fl_fonts) + fnum;
  return &(s->first);
}
#endif

Fl_Vk_Choice *Fl_Wayland_Vk_Window_Driver::find(int m, const int *alistp) {
  Fl_Wayland_Vk_Choice *g = (Fl_Wayland_Vk_Choice *)Fl_Vk_Window_Driver::find_begin(m, alistp);
  if (g)
    return g;

  g = new Fl_Wayland_Vk_Choice(m, alistp, first);
  first = g;
  return g;
}


float Fl_Wayland_Vk_Window_Driver::pixels_per_unit() {
    int ns = pWindow->screen_num();
    int wld_scale = (pWindow->shown() ?
                     Fl_Wayland_Window_Driver::driver(pWindow)->wld_scale() : 1);
    return wld_scale * Fl::screen_driver()->scale(ns);
}


int Fl_Wayland_Vk_Window_Driver::mode_(int m, const int *a) {
  mode(m | FL_DOUBLE);
  return 1;
}

void Fl_Wayland_Vk_Window_Driver::swap_buffers() {
  // like issue #967, but on Vulkan see #1292  -- this solves it
  if (pWindow->m_surface != VK_NULL_HANDLE)
  {
    if (pWindow->parent()) { 
      struct wld_window* window = fl_wl_xid(pWindow);
      if (window->frame_cb || !window->wl_surface) return;

      // Force only if totally off-screen
      if (wl_list_empty(&window->outputs)) {
          window->frame_cb = wl_surface_frame(window->wl_surface);
          wl_callback_add_listener(window->frame_cb, Fl_Wayland_Graphics_Driver::p_surface_frame_listener, window);
          wl_display_flush(fl_wl_display());
      }
    }
  }
}

// ---------------------------------------------------------------------------
// resize()
//
// BUG FIX: moving a window slowly between screens with different DPI scales
// (e.g. 1× → 2×) used to produce a fatal Wayland protocol error:
//
//   wl_surface#N: error 2: Buffer size (WxH) must be an integer multiple
//   of the buffer_scale (2).
//
// Root cause
// ----------
// When Wayland reports a scale change FLTK calls resize() which previously
// called wl_surface_set_buffer_scale(newScale) immediately.  The existing
// swapchain images (whose pixel dimensions were computed for the OLD scale)
// are still alive at this point.  On the very next vkQueuePresentKHR the
// compositor checks: buffer_scale × logical_size == buffer_size.  If the
// old pixel dimensions are not exact multiples of newScale (e.g. 933×29 is
// not a multiple of 2) it fires the fatal protocol error.
//
// Fix
// ---
// wl_surface_set_buffer_scale() must only be called when NO live swapchain
// images exist for the current surface.  That condition holds precisely
// inside prepare(): destroy_resources() clears pWindow->m_buffers before
// prepare() is called, so m_buffers.empty() is a reliable, zero-overhead
// guard.  When resize() is called from an external event (m_buffers not
// empty) and the scale is changing we record the new desired scale and flag
// the swapchain for recreation; the actual wl_surface_set_buffer_scale()
// call is then made safely inside the next prepare() invocation, right
// before fresh, correctly-aligned images are built.
// ---------------------------------------------------------------------------
void Fl_Wayland_Vk_Window_Driver::resize(int is_a_resize, int W, int H) {
    int new_scale = Fl_Wayland_Window_Driver::driver(pWindow)->wld_scale();
    struct wld_window *xid = fl_wl_xid(pWindow);
    if (!xid) return;

    // Always set up the frame callback — this is safe regardless of scale.
    if (xid->kind == Fl_Wayland_Window_Driver::DECORATED && !xid->frame_cb) {
        xid->frame_cb = wl_surface_frame(xid->wl_surface);
        wl_callback_add_listener(xid->frame_cb,
                                 Fl_Wayland_Graphics_Driver::p_surface_frame_listener, xid);
    }

    // Guard: if live swapchain images are present (m_buffers non-empty) and
    // the buffer scale is changing, do NOT call wl_surface_set_buffer_scale()
    // yet.  Instead, record the new desired scale (used by prepare_buffers()
    // for alignment) and request a swapchain recreation.  The scale will be
    // applied safely on the next prepare() call, after the old images are
    // retired and new aligned ones are created.
    //
    // Note: during prepare() (called from recreate_swapchain()) m_buffers has
    // already been cleared by destroy_resources(), so the guard below is false
    // and we proceed to update the surface scale immediately — which is correct
    // because prepare_buffers() follows right after and creates properly-aligned
    // images before any present calls can happen.
    if (!pWindow->m_buffers.empty() && new_scale != m_current_wld_scale) {
        m_current_wld_scale = new_scale;          // remember for alignment in prepare_buffers()
        pWindow->reinit_swapchain();
        return;  // defer wl_surface_set_buffer_scale() to the next prepare()
    }

    // Safe to apply the scale now:
    //  • First-time setup (m_current_wld_scale == 0): no swapchain exists yet.
    //  • Scale unchanged: nothing to do alignment-wise.
    //  • Called from within prepare(): old images already destroyed.
    m_current_wld_scale = new_scale;
    wl_surface_set_buffer_scale(xid->wl_surface, new_scale);
}


void *Fl_Wayland_Vk_Window_Driver::GetProcAddress(const char *procName) {
  return dlsym(RTLD_DEFAULT, procName);
}

void Fl_Wayland_Vk_Window_Driver::create_surface() {
  VkWaylandSurfaceCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;

  struct wl_display* display = fl_wl_display();
  if (!display) Fl::fatal("Wayland display is nullptr!");

  createInfo.display = display;

  struct wl_surface* surface = fl_wl_surface(fl_wl_xid(pWindow));
  if (!surface) Fl::fatal("Wayland surface is nullptr!");

  createInfo.surface = surface;
  
  if (vkCreateWaylandSurfaceKHR(pWindow->ctx.instance, &createInfo, nullptr,
                                &pWindow->m_surface) !=
      VK_SUCCESS) {
    Fl::fatal("Failed to create Wayland surface!");
  }
}

void Fl_Wayland_Vk_Window_Driver::make_current_before() {
}


std::vector<const char*> Fl_Wayland_Vk_Window_Driver::get_instance_extensions() {
  std::vector<const char*> out;
  out.push_back("VK_KHR_surface");
  out.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
  return out;
}


int Fl_Wayland_Vk_Window_Driver::flush_begin() {
    struct wld_window* window = fl_wl_xid(pWindow);
    if (window && window->frame_cb && window->fl_win)
    {
        return 1;  // we have a callback, skip this frame
    }
    return 0;
}

// ---------------------------------------------------------------------------
// get_surface_buffer_scale()
//
// Returns the buffer scale that was most recently applied (or will be applied
// on the next prepare() call) to the Wayland surface.  This is consumed by
// prepare_buffers() in the common driver to align the swapchain extent, which
// guards against the compositor-provided currentExtent being temporarily
// unaligned while the window straddles two monitors during a slow drag.
// ---------------------------------------------------------------------------
int Fl_Wayland_Vk_Window_Driver::get_surface_buffer_scale() const {
    return (m_current_wld_scale > 0) ? m_current_wld_scale : 1;
}

Fl_Wayland_Vk_Window_Driver::~Fl_Wayland_Vk_Window_Driver() {}


#endif // HAVE_VK
