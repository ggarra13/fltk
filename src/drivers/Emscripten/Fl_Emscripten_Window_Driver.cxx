//
// Implementation of the Emscripten window driver.
//
// Copyright 1998-2024 by Bill Spitzak and others.
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


#include "Fl_Emscripten_Window_Driver.H"
#include "../../Fl_Window_Driver.H"
#include "../../Fl_Screen_Driver.H"
#include "Fl_Emscripten_Graphics_Driver.H"
#include <FL/Enumerations.H>
#include <FL/platform.H>
#include <FL/platform_types.h>
#include <emscripten.h>
#include <emscripten/val.h>
#include <emscripten/html5.h>
#include <FL/names.h>

using namespace emscripten;

static int ID = 1;

Window fl_window;

const char *special_keys[] = {"Backspace",     "Tab",     "IsoKey",    "Enter",    "Pause",
                              "ScrollLock",    "Escape",  "Kana",      "Eisu",     "Yen",
                              "JISUnderscore", "Home",    "ArrowLeft", "ArrowUp",  "ArrowRight",
                              "ArrowDown",     "PageUp",  "PageDown",  "End",      "Print",
                              "Insert",        "Menu",    "Help",      "NumLock",  "KP",
                              "KPEnter",       "KPLast",  "F",         "FLast",    "Shift",
                              "Shift",         "Control", "Control",   "CapsLock", "Meta",
                              "Meta",          "Alt",     "Alt",       "Delete",   "AltGr", "AltGraph"};

int special_keys_equiv[] = {FL_BackSpace,      FL_Tab,       FL_Iso_Key,   FL_Enter,     FL_Pause,
                            FL_Scroll_Lock,    FL_Escape,    FL_Kana,      FL_Eisu,      FL_Yen,
                            FL_JIS_Underscore, FL_Home,      FL_Left,      FL_Up,        FL_Right,
                            FL_Down,           FL_Page_Up,   FL_Page_Down, FL_End,       FL_Print,
                            FL_Insert,         FL_Menu,      FL_Help,      FL_Num_Lock,  FL_KP,
                            FL_KP_Enter,       FL_KP_Last,   FL_F,         FL_F_Last,    FL_Shift_L,
                            FL_Shift_R,        FL_Control_L, FL_Control_R, FL_Caps_Lock, FL_Meta_L,
                            FL_Meta_R,         FL_Alt_L,     FL_Alt_R,     FL_Delete,    FL_Alt_Gr, FL_Alt_Gr};

static int special_key(const char *k, bool right) {
  for (int i = 0; i < sizeof(special_keys) / sizeof(special_keys[0]); i++) {
    if (!strcmp(special_keys[i], k)) {
      return i + (right ? 1: 0);
    }
  }
  return -1;
}

static void set_keysym_and_state(const EmscriptenKeyboardEvent *keyEvent) {
  ulong state = 0;
  int temp = special_key(keyEvent->key, keyEvent->location == 2);
  if (temp >= 0) {
    Fl::e_keysym = special_keys_equiv[temp];
    Fl::e_length = 0;
    Fl::e_text = (char *)"";
  } else {
    Fl::e_keysym = keyEvent->key[0];
    Fl::e_length = strlen(keyEvent->key);
    Fl::e_text = (char *)keyEvent->key;
  }
  if (keyEvent->ctrlKey)
    state |= FL_CTRL;
  if (keyEvent->altKey)
    state |= FL_ALT;
  if (keyEvent->shiftKey)
    state |= FL_SHIFT;
  if (keyEvent->metaKey)
    state |= FL_META;
  Fl::e_state = state;
}


static int match_mouse_event(int eventType) {
  switch (eventType) {
    case EMSCRIPTEN_EVENT_MOUSEDOWN:
      return FL_PUSH;
    case EMSCRIPTEN_EVENT_MOUSEUP:
      return FL_RELEASE;
    case EMSCRIPTEN_EVENT_MOUSEMOVE:
      return FL_MOVE;
    case EMSCRIPTEN_EVENT_MOUSEENTER:
      return FL_ENTER;
    case EMSCRIPTEN_EVENT_MOUSELEAVE:
      return FL_LEAVE;
    case EMSCRIPTEN_EVENT_DBLCLICK:
      return FL_PUSH;
    case EMSCRIPTEN_EVENT_CLICK:
      return FL_PUSH;
    default:
      return 0;
  }
}

static int match_key_event(int eventType) {
  switch (eventType) {
    case EMSCRIPTEN_EVENT_KEYPRESS:
    case EMSCRIPTEN_EVENT_KEYDOWN:
      return FL_KEYDOWN;
    case EMSCRIPTEN_EVENT_KEYUP:
      return FL_KEYUP;
    default:
      return 0;
  }
}

static int px, py;

static bool handle_mouse_ev(int eventType, const EmscriptenMouseEvent *event, void *data) {
  Fl_Window *pWindow = (Fl_Window *)data;
  ulong state = Fl::e_state & 0xff0000;
  Fl::e_x = event->targetX;
  Fl::e_y = event->targetY;
  Fl::e_x_root = event->clientX;
  Fl::e_y_root = event->clientY;
  int flev = match_mouse_event(eventType);
  if (flev == FL_PUSH) {
    if (eventType == EMSCRIPTEN_EVENT_DBLCLICK)
      Fl::e_clicks = 1;
    else
      Fl::e_clicks = 0;
    Fl::e_is_click = 1;
    px = Fl::e_x_root;
    py = Fl::e_y_root;
    if (event->button == 0)
      state |= FL_BUTTON1;
    if (event->button == 1)
      state |= FL_BUTTON2;
    if (event->button == 2)
      state |= FL_BUTTON3;
    if (event->button == 3)
      state |= FL_BUTTON4;
    if (event->button == 4)
      state |= FL_BUTTON5;
    Fl::e_keysym = FL_Button + event->button + 1;
  } else if (flev == FL_RELEASE) {
    if (abs(Fl::e_x_root - px) > 5 || abs(Fl::e_y_root - py) > 5)
      Fl::e_is_click = 0;
    if (event->button == 0)
      state &= ~FL_BUTTON1;
    if (event->button == 1)
      state &= ~FL_BUTTON2;
    if (event->button == 2)
      state &= ~FL_BUTTON3;
    if (event->button == 3)
      state &= ~FL_BUTTON4;
    if (event->button == 4)
      state &= ~FL_BUTTON5;
    Fl::e_keysym = FL_Button + event->button + 1;
  }
  Fl::e_state = state;
  int ret = Fl::handle(flev, pWindow);
  return ret;
}

static bool handle_wheel_ev(int eventType, const EmscriptenWheelEvent *event, void *data) {
  Fl_Window *pWindow = (Fl_Window *)data;
  Fl::e_x = event->mouse.targetX;
  Fl::e_y = event->mouse.targetY;
  Fl::e_x_root = event->mouse.clientX;
  Fl::e_y_root = event->mouse.clientY;
  Fl::e_dx = event->deltaX / 80.0;
  Fl::e_dy = event->deltaY / 80.0;
  int ret = Fl::handle(FL_MOUSEWHEEL, pWindow);
  pWindow->redraw();
  return ret;
}

static bool handle_key_ev(int eventType, const EmscriptenKeyboardEvent *event, void *data) {
  Fl_Window *pWindow = (Fl_Window *)data;
  set_keysym_and_state(event);
  int flev = match_key_event(eventType);
  int ret = 0;
  ret = Fl::handle(flev, pWindow);
  return ret;
}


extern "C" EMSCRIPTEN_KEEPALIVE void handle_unfocus(int id) {
  Fl_Window *w = fl_em_find(id);
  EM_ASM(
      {
        let div = document.getElementById("fltk_div" + $0);
        if (div) {
          div.style.zIndex = 0;
        }
      },
      id);
  Fl::handle(FL_UNFOCUS, w);
}

extern "C" EMSCRIPTEN_KEEPALIVE void handle_focus(int id) {
  Fl_Window *w = fl_em_find(id);
  EM_ASM(
      {
        let div = document.getElementById("fltk_div" + $0);
        let canvas = document.getElementById("fltk_canvas" + $0);
        if (div && canvas) {
          div.style.zIndex = 1;
          canvas.focus();
        }
      },
      id);
  Fl::handle(FL_FOCUS, w);
}

#define REGISTER_MOUSE_EV(ev) \
  emscripten_set_##ev##_callback(canvasid, pWindow, true, handle_mouse_ev);

#define REGISTER_WHEEL_EV(ev) \
  emscripten_set_##ev##_callback(canvasid, pWindow, true, handle_wheel_ev);

#define REGISTER_KEY_EV(ev) emscripten_set_##ev##_callback(canvasid, pWindow, true, handle_key_ev);

#define UNREGISTER_EV(ev) emscripten_set_##ev##_callback(canvasid, pWindow, true, NULL);

Fl_Emscripten_Window_Driver::Fl_Emscripten_Window_Driver(Fl_Window *w)
  : Fl_Window_Driver(w), cursor(NULL) {}

extern "C" void EMSCRIPTEN_KEEPALIVE fltk_em_track_div(int xid, int x, int y) {
  Fl_Window *win = fl_em_find(xid);
  win->position(x, y + 30);
}


// Basically the window itself is a div element. Decorations for bordered windows are done in the browser using a div as well.
// The window div contains a canvas element which is the FLTK window.
void Fl_Emscripten_Window_Driver::makeWindow() {
  Fl_Group::current(0);
  if (pWindow->parent() && !pWindow->window())
    return;
  if (pWindow->parent() && !pWindow->window()->shown())
    return;
  if (pWindow->tooltip_window()) {
    force_position(1);
    x(Fl::e_x_root);
    y(Fl::e_y_root - 30);
  }
  wait_for_expose_value = 1;
  // clang-format off
  EM_ASM(
      {
        let body = document.getElementsByTagName("body")[0];
        let div = document.createElement("DIV");
        div.id = "fltk_div" + $0;
        div.tabIndex = "-1";
        div.addEventListener("contextmenu", (e) => e.preventDefault());
        div.style.position = "absolute";
        div.style.left = $2 + "px";
        div.style.top = $1 ? ($3 - 30) + "px" : $3 + "px";
        div.style.zIndex = 1;
        div.style.backgroundColor = "#f1f1f1";
        div.style.borderRight = "1px solid #555";
        div.style.borderBottom = "1px solid #555";
        div.style.textAlign = "center";
        body.appendChild(div);
        let decor = document.createElement("DIV");
        decor.id = "fltk_decor" + $0;
        decor.style.height = "16px";
        decor.style.font = "14px Arial";
        decor.style.padding = "6px";
        decor.style.cursor = "move";
        decor.style.zIndex = 2;
        decor.style.backgroundColor = "#2196F3";
        decor.style.color = "#fff";
        decor.style.cursor = "pointer";
        div.appendChild(decor);
        let header = document.createElement("DIV");
        header.textContent = UTF8ToString($6);
        header.id = "fltk_decor_header" + $0;
        header.style.font = "14px Arial";
        decor.appendChild(header);
        let close = document.createElement("BUTTON");
        close.id = "closewin";
        close.textContent = "X";
        close.style.font = "bold 14px Arial";
        close.style.position = "absolute";
        close.style.top = "1%";
        close.style.right = "1px";
        close.style.backgroundColor = "#2196F3";
        close.style.border = "none";
        close.style.color = "#fff";
        close.addEventListener("click", () => div.hidden = true);
        decor.appendChild(close);
        let canvas = document.createElement("CANVAS");
        canvas.id = "fltk_canvas" + $0;
        canvas.setAttribute("data-raw-handle", $0.toString());
        canvas.tabIndex = "-1";
        canvas.width = $4;
        canvas.height = $5;
        div.appendChild(canvas);
        canvas.addEventListener("click", () => canvas.focus());
        decor.addEventListener("mousedown", () => canvas.focus());
        div.addEventListener(
            "focusin", () => { canvas.focus(); div.style.zIndex = 1; });
        div.addEventListener(
            "focusout", () => { canvas.blur(); div.style.zIndex = 0; });
        if ($1 === 0) decor.hidden = true;
        // https://www.w3schools.com/HOWTO/howto_js_draggable.asp
        function dragElement(elmnt) {
          var pos1 = 0;
          var pos2 = 0;
          var pos3 = 0;
          var pos4 = 0;
          if (document.getElementById("fltk_decor" + $0)) {
            document.getElementById("fltk_decor" + $0).onmousedown = dragMouseDown;
          } else {
            elmnt.onmousedown = dragMouseDown;
          }

          function dragMouseDown(e) {
            e = e || window.event;
            e.preventDefault();
            pos3 = e.clientX;
            pos4 = e.clientY;
            document.onmouseup = closeDragElement;
            document.onmousemove = elementDrag;
          }

          function elementDrag(e) {
            e = e || window.event;
            e.preventDefault();
            pos1 = pos3 - e.clientX;
            pos2 = pos4 - e.clientY;
            pos3 = e.clientX;
            pos4 = e.clientY;
            elmnt.style.left = (elmnt.offsetLeft - pos1) + "px";
            elmnt.style.top = (elmnt.offsetTop - pos2) + "px";
            _fltk_em_track_div($0, elmnt.offsetLeft|0, elmnt.offsetTop|0);
          }

          function closeDragElement() {
            document.onmouseup = null;
            document.onmousemove = null;
          }
        }

        dragElement(document.getElementById("fltk_div" + $0));
      },
      ID, pWindow->border(), pWindow->x(), pWindow->y(), 
      pWindow->w(), pWindow->h(), pWindow->label() ? pWindow->label(): "");
  // clang-format on
  Fl_X *xp = new Fl_X;
  xp->xid = (fl_uintptr_t)ID;
  other_xid = 0;
  xp->w = pWindow;
  flx(xp);
  xp->region = 0;
  if (!pWindow->parent()) {
    xp->next = Fl_X::first;
    Fl_X::first = xp;
  } else if (Fl_X::first) {
    xp->next = Fl_X::first->next;
    Fl_X::first->next = xp;
  } else {
    xp->next = NULL;
    Fl_X::first = xp;
  }
  fl_window = (Window)ID;
  pWindow->set_visible();
  wait_for_expose_value = 0;
  char canvasid[32];
  sprintf(canvasid, "#%s%d", "fltk_canvas", ID);
  REGISTER_MOUSE_EV(mousedown);
  REGISTER_MOUSE_EV(mouseup);
  REGISTER_MOUSE_EV(mousemove);
  REGISTER_MOUSE_EV(mouseenter);
  REGISTER_MOUSE_EV(mouseleave);
  REGISTER_MOUSE_EV(dblclick);
  REGISTER_WHEEL_EV(wheel);
  REGISTER_KEY_EV(keydown);
  REGISTER_KEY_EV(keyup);
  int old_event = Fl::e_number;
  pWindow->redraw();
  pWindow->handle(Fl::e_number = FL_SHOW); // get child windows to appear
  Fl::e_number = old_event;
  if (ID == 1) {
    if (!strcmp(pWindow->xclass(), "./this.program"))
      emscripten_set_window_title("FLTK");
    else
      emscripten_set_window_title(pWindow->xclass());
  }
  ID += 1;
}

void Fl_Emscripten_Window_Driver::show() {
  if (!shown()) {
    fl_open_display();
    makeWindow();
  } else {
    fl_uintptr_t xid = Fl_X::flx(pWindow)->xid;
    EM_ASM(
        {
          let div = document.getElementById("fltk_div" + $0);
          div.hidden = false;
        },
        (int)xid);
    Fl::handle(FL_SHOW, pWindow);
  }
}

void Fl_Emscripten_Window_Driver::map() {
  Fl_X *ip = Fl_X::flx(pWindow);
  EM_ASM(
      {
        let div = document.getElementById("fltk_div" + $0);
        if (div) {
          div.hidden = false;
          div.style.zIndex = 1;
        }
      },
      (int)ip->xid);
}

extern "C" void EMSCRIPTEN_KEEPALIVE delete_div(int xid) {
  Fl_Window *pWindow = fl_em_find(xid);
  char canvasid[32];
  sprintf(canvasid, "#%s%d", "fltk_canvas", xid);
  UNREGISTER_EV(mousedown);
  UNREGISTER_EV(mouseup);
  UNREGISTER_EV(mousemove);
  UNREGISTER_EV(mouseenter);
  UNREGISTER_EV(mouseleave);
  UNREGISTER_EV(dblclick);
  UNREGISTER_EV(wheel);
  UNREGISTER_EV(keydown);
  UNREGISTER_EV(keyup);
  EM_ASM(
      {
        let div = document.getElementById("fltk_div" + $0);
        if (div) {
          div.remove();
        }
      },
      (int)xid);
}

void Fl_Emscripten_Window_Driver::hide() {
  Fl_X *ip = Fl_X::flx(pWindow);
  delete_div((int)ip->xid);
  if (hide_common())
    return;
  if (ip->xid == (fl_uintptr_t)fl_window)
    fl_window = 0;
  delete ip;
}

void Fl_Emscripten_Window_Driver::unmap() {
  Fl_X *ip = Fl_X::flx(pWindow);
  EM_ASM(
      {
        let div = document.getElementById("fltk_div" + $0);
        if (div) {
          div.hidden = true;
        }
      },
      (int)ip->xid);
}

void Fl_Emscripten_Window_Driver::make_current() {
  fl_uintptr_t xid = Fl_X::flx(pWindow)->xid;
  EM_VAL ctx = (EM_VAL)EM_ASM_PTR({
    let canvas = document.getElementById("fltk_canvas" + $0);
    let ctx = canvas.getContext("2d");
    return Emval.toHandle(ctx);
  }, (int)xid);
  ((Fl_Emscripten_Graphics_Driver *)fl_graphics_driver)->context(ctx);
  fl_window = (Window)xid;
}

void Fl_Emscripten_Window_Driver::use_border() {
  if (!shown() || pWindow->parent())
    return;
  pWindow->wait_for_expose();
  fl_uintptr_t xid = Fl_X::flx(pWindow)->xid;
  EM_ASM(
      {
        let decor = document.getElementById("fltk_decor" + $0);
        decor.hidden = !decor.hidden;
      },
      (int)xid);
  Fl_Window_Driver::use_border();
}

void Fl_Emscripten_Window_Driver::resize(int X, int Y, int W, int H) {
  int is_a_rescale = Fl_Window::is_a_rescale();
  int is_a_move = (X != x() || Y != y() || is_a_rescale);
  int is_a_resize = (W != w() || H != h() || is_a_rescale);
  if (is_a_move)
    force_position(1);
  else if (!is_a_resize && !is_a_move)
    return;
  pWindow->wait_for_expose();
  if (is_a_resize) {
    pWindow->Fl_Group::resize(X, Y, W, H);
    if (pWindow->shown()) {
      fl_uintptr_t xid = Fl_X::flx(pWindow)->xid;
      EM_ASM(
          {
            let X = $1;
            let Y = $2;
            let W = $3;
            let H = $4;
            let canvas = document.getElementById("fltk_canvas" + $0);
            if (canvas) {
              canvas.width = W;
              canvas.height = H;
            }
            let div = document.getElementById("fltk_div" + $0);
            if (div) {
              div.style.left = X + "px";
              div.style.top = Y + "px";
            }
          },
          (int)xid, X, Y, W, H);
      // divs aren't resizable?
    }
    pWindow->redraw();
  } else {
    x(X);
    y(Y);
  }
}

void Fl_Emscripten_Window_Driver::fullscreen_on() {
  pWindow->_set_fullscreen();
  pWindow->border(0);
  resize(0, 0, Fl::screen_driver()->w(), Fl::screen_driver()->h());
  Fl::handle(FL_FULLSCREEN, pWindow);
}

void Fl_Emscripten_Window_Driver::fullscreen_off(int X, int Y, int W, int H) {
  pWindow->_clear_fullscreen();
  resize(X, Y, W, H);
  pWindow->border(1);
  Fl::handle(FL_FULLSCREEN, pWindow);
}

void Fl_Emscripten_Window_Driver::take_focus() {
  fl_uintptr_t xid = Fl_X::flx(pWindow)->xid;
  handle_focus((int)xid);
}


int Fl_Emscripten_Window_Driver::set_cursor(Fl_Cursor c) {
  if (cursor) {
    cursor = NULL;
  }

  switch (c) {
    case FL_CURSOR_DEFAULT:
      cursor = "default";
      break;
    case FL_CURSOR_ARROW:
      cursor = "default";
      break;
    case FL_CURSOR_CROSS:
      cursor = "crosshair";
      break;
    case FL_CURSOR_INSERT:
      cursor = "text";
      break;
    case FL_CURSOR_HAND:
      cursor = "pointer";
      break;
    case FL_CURSOR_MOVE:
      cursor = "move";
      break;
    case FL_CURSOR_WAIT:
      cursor = "wait";
      break;
    case FL_CURSOR_HELP:
      cursor = "help";
      break;
    case FL_CURSOR_NS:
      cursor = "ns-resize";
      break;
    case FL_CURSOR_WE:
      cursor = "ew-resize";
      break;
    case FL_CURSOR_N:
      cursor = "n-resize";
      break;
    case FL_CURSOR_E:
      cursor = "e-resize";
      break;
    case FL_CURSOR_W:
      cursor = "w-resize";
      break;
    case FL_CURSOR_S:
      cursor = "s-resize";
      break;
    case FL_CURSOR_NESW:
      cursor = "nesw-resize";
      break;
    case FL_CURSOR_NWSE:
      cursor = "nwse-resize";
      break;
    case FL_CURSOR_NONE:
      cursor = "none";
      break;
    default:
      return 0;
  }

  EM_ASM({ document.body.style.cursor = UTF8ToString($0); }, cursor);

  return 1;
}

void Fl_Emscripten_Window_Driver::flush() {
  if (!pWindow->damage()) return;
  pWindow->redraw();
  Fl_Window_Driver::flush();
}

Fl_Window *fl_em_find(int xid) {
  return Fl_Window_Driver::find((fl_uintptr_t)xid);
}


int fl_em_xid(const Fl_Window *win) {
  return (int)Fl_Window_Driver::xid(win);
}

EM_VAL fl_em_gc() {
  return ((Fl_Emscripten_Graphics_Driver*)fl_graphics_driver)->context();
}
