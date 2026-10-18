//
// Class Fl_Emscripten_Gl_Window_Driver for the Fast Light Tool Kit (FLTK).
//
// Copyright 2021-2025 by Bill Spitzak and others.
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
#if HAVE_GL
#include <stdio.h>
#include <FL/platform.H>
#include <FL/Fl_Gl_Window.H>
#include "../../Fl_Gl_Choice.H"
#include "../../Fl_Screen_Driver.H"
#include "Fl_Emscripten_Gl_Window_Driver.H"
#include "../../Fl_Scalable_Graphics_Driver.H" // Fl_Font_Descriptor
#include <emscripten/html5.h>
#include <emscripten/emscripten.h>

// Describes context choices and holds translated WebGL attributes.
class Fl_Emscripten_Gl_Choice : public Fl_Gl_Choice {
  friend class Fl_Emscripten_Gl_Window_Driver;
private:
  EmscriptenWebGLContextAttributes attrs;
  bool need_gl3;            // FL_OPENGL3 requested: WebGL2 required, no fallback
  mutable int actual_major; // 2 or 1 after create_gl_context(); 0 = not created yet
public:
  Fl_Emscripten_Gl_Choice(int m, const int *alistp, Fl_Gl_Choice *n)
    : Fl_Gl_Choice(m, alistp, n), need_gl3((m & FL_OPENGL3) != 0), actual_major(0) {
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.alpha   = (m & FL_ALPHA) ? 1 : 0;
    attrs.depth   = ((m & FL_DEPTH) || (m & FL_DEPTH32)) ? 1 : 0;
    attrs.stencil = (m & FL_STENCIL) ? 1 : 0;
    attrs.antialias = (m & FL_MULTISAMPLE) ? 1 : 0;
    attrs.enableExtensionsByDefault = 1;
    // WebGL clears the drawing buffer after compositing. Single-buffered FLTK
    // windows expect their content to persist, so preserve it for them.
    attrs.preserveDrawingBuffer = (m & FL_DOUBLE) ? 0 : 1;
    // Be explicit (this is also the WebGL default).
    attrs.premultipliedAlpha = 1;
    attrs.majorVersion = 2;
    attrs.minorVersion = 0;
  }
};

// Context loss / restore: after a loss all GL objects are gone, so make FLTK
// run the window's "!valid()" initialization path again.
static EM_BOOL fl_webgl_context_lost_cb(int, const void *, void *) {
  // Returning EM_TRUE calls preventDefault(), which allows the browser to
  // restore the context later.
  return EM_TRUE;
}

static EM_BOOL fl_webgl_context_restored_cb(int, const void *, void *user_data) {
  Fl_Gl_Window *w = (Fl_Gl_Window *)user_data;
  if (w) {
    w->invalidate();   // valid(0) + context invalid -> app re-creates its GL resources
    w->redraw();
  }
  return EM_TRUE;
}

static void fl_make_canvas_target(Fl_Window *window, char *buf, size_t len) {
  fl_uintptr_t xid = fl_xid(window);
  snprintf(buf, len, "#fltk_canvas%lu", (unsigned long)xid);
}

Fl_Gl_Window_Driver *Fl_Gl_Window_Driver::newGlWindowDriver(Fl_Gl_Window *w)
{
  return new Fl_Emscripten_Gl_Window_Driver(w);
}

// Legacy display-list based text is not available in WebGL (and not emulated by
// LEGACY_GL_EMULATION). Text drawing via gl_draw() therefore needs a
// texture-atlas implementation; until then these are deliberate no-ops.
void Fl_Emscripten_Gl_Window_Driver::draw_string_legacy(const char* str, int n) {
  (void)str; (void)n;
}

int Fl_Emscripten_Gl_Window_Driver::genlistsize() {
  return 256;
}

void Fl_Emscripten_Gl_Window_Driver::gl_bitmap_font(Fl_Font_Descriptor *fl_fontsize) {
  (void)fl_fontsize;
}

void Fl_Emscripten_Gl_Window_Driver::get_list(Fl_Font_Descriptor *fd, int r) {
  (void)fd; (void)r;
}

Fl_Gl_Choice *Fl_Emscripten_Gl_Window_Driver::find(int m, const int *alistp)
{
  // Reject what WebGL cannot provide so Fl_Gl_Window::can_do() is truthful.
  // NOTE: when an alist is used, m may be 0 here; alist parsing is not done.
  if (m & (FL_STEREO | FL_ACCUM)) return 0;

  Fl_Gl_Choice *g = Fl_Gl_Window_Driver::find_begin(m, alistp);
  if (g) return g;

  g = new Fl_Emscripten_Gl_Choice(m, alistp, first);
  first = g;
  return g;
}

GLContext Fl_Emscripten_Gl_Window_Driver::create_gl_context(Fl_Window* window,
                                                            const Fl_Gl_Choice* g) {
  if (!g || !window) return 0;
  const Fl_Emscripten_Gl_Choice *choice = static_cast<const Fl_Emscripten_Gl_Choice*>(g);

  char target[64];
  fl_make_canvas_target(window, target, sizeof(target));

  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context(target, &choice->attrs);
  int major = choice->attrs.majorVersion;

  if (ctx <= 0 && !choice->need_gl3 && choice->attrs.majorVersion == 2) {
    // WebGL2 unavailable: fall back to WebGL1 unless the app required GL3.
    EmscriptenWebGLContextAttributes fallback_attrs = choice->attrs;
    fallback_attrs.majorVersion = 1;
    ctx = emscripten_webgl_create_context(target, &fallback_attrs);
    major = 1;
  }

  if (ctx <= 0) return 0;

  choice->actual_major = major; // query with Fl_Emscripten_Gl_Choice / glGetString(GL_VERSION)

  emscripten_set_webglcontextlost_callback(target, window, EM_TRUE, fl_webgl_context_lost_cb);
  emscripten_set_webglcontextrestored_callback(target, window, EM_TRUE, fl_webgl_context_restored_cb);

  add_context((GLContext)ctx);
  return (GLContext)ctx;
}

void Fl_Emscripten_Gl_Window_Driver::set_gl_context(Fl_Window* w, GLContext context) {
  if (!context) return;
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE current_context = emscripten_webgl_get_current_context();
  if ((EMSCRIPTEN_WEBGL_CONTEXT_HANDLE)context != current_context || w != cached_window) {
    if (emscripten_webgl_make_context_current((EMSCRIPTEN_WEBGL_CONTEXT_HANDLE)context)
        == EMSCRIPTEN_RESULT_SUCCESS) {
      cached_window = w;
    }
  }
}

void Fl_Emscripten_Gl_Window_Driver::delete_gl_context(GLContext context) {
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE current_context = emscripten_webgl_get_current_context();
  if (current_context == (EMSCRIPTEN_WEBGL_CONTEXT_HANDLE)context) {
    cached_window = 0;
    emscripten_webgl_make_context_current(0);
  }

  // Unregister loss/restore callbacks (so they never see a dangling window).
  if (pWindow && fl_xid(pWindow)) {
    char target[64];
    fl_make_canvas_target(pWindow, target, sizeof(target));
    emscripten_set_webglcontextlost_callback(target, 0, EM_TRUE, 0);
    emscripten_set_webglcontextrestored_callback(target, 0, EM_TRUE, 0);
  }

  emscripten_webgl_destroy_context((EMSCRIPTEN_WEBGL_CONTEXT_HANDLE)context);
  del_context(context);
}

// WebGL has no front buffer and no overlay planes: deliberate no-ops.
// (can_do_overlay() should stay false.)
void Fl_Emscripten_Gl_Window_Driver::make_overlay_current() {
}

void Fl_Emscripten_Gl_Window_Driver::redraw_overlay() {
  pWindow->damage(FL_DAMAGE_OVERLAY);
}

void Fl_Emscripten_Gl_Window_Driver::before_show(int&) {
    //Fl_X::make_xid(pWindow, 0, 0);
}

float Fl_Emscripten_Gl_Window_Driver::pixels_per_unit()
{
  int ns = pWindow->screen_num();
  return Fl::screen_driver()->scale(ns);
}

int Fl_Emscripten_Gl_Window_Driver::mode_(int m, const int *a) {
  int oldmode = mode();
  mode(m); alist(a);
  if (pWindow->shown()) {
    g( find(m, a) );
    // Context attributes cannot change on an existing canvas, so recreate the
    // window when attributes that affect the context change.
    // VERIFY: show() must create a fresh canvas element.
    const int ctx_bits = FL_DOUBLE | FL_ALPHA | FL_DEPTH | FL_DEPTH32 |
                         FL_STENCIL | FL_MULTISAMPLE | FL_OPENGL3;
    if ((oldmode ^ m) & ctx_bits) {
      pWindow->hide();
      pWindow->show();
    }
  } else {
    g(0);
  }
  return 1;
}

void Fl_Emscripten_Gl_Window_Driver::swap_buffers() {
  // WebGL presents implicitly when control returns to the browser.
  // (Only call emscripten_webgl_commit_frame() if explicitSwapControl is enabled.)
}

char Fl_Emscripten_Gl_Window_Driver::swap_type() {
  // Double-buffered WebGL content is undefined after presentation, so FLTK
  // must redraw everything. Single-buffered windows preserve their buffer.
  // VERIFY: enum name (UNDEFINED / COPY) in Fl_Gl_Window_Driver.
  return copy;
}

void Fl_Emscripten_Gl_Window_Driver::swap_interval(int interval) {
  (void)interval; // tied to requestAnimationFrame
}

int Fl_Emscripten_Gl_Window_Driver::swap_interval() const {
  return -1;
}

void Fl_Emscripten_Gl_Window_Driver::waitGL() {
}

void Fl_Emscripten_Gl_Window_Driver::gl_visual(Fl_Gl_Choice *c) {
  Fl_Gl_Window_Driver::gl_visual(c);
}

void Fl_Emscripten_Gl_Window_Driver::gl_start() {
}

#endif // HAVE_GL
