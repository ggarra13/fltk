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
#include <FL/platform.H>
#include "../../Fl_Gl_Choice.H"
#include "../../Fl_Screen_Driver.H"
#include "Fl_Emscripten_Gl_Window_Driver.H"
#include "../../Fl_Scalable_Graphics_Driver.H" // Fl_Font_Descriptor
#include <emscripten/html5.h>
#include <emscripten/emscripten.h>

// Describes context choices needed for Emscripten WebGL.
class Fl_Emscripten_Gl_Choice : public Fl_Gl_Choice {
  friend class Fl_Emscripten_Gl_Window_Driver;
public:
  Fl_Emscripten_Gl_Choice(int m, const int *alistp, Fl_Gl_Choice *n) : Fl_Gl_Choice(m, alistp, n) {}
};

Fl_Gl_Window_Driver *Fl_Gl_Window_Driver::newGlWindowDriver(Fl_Gl_Window *w)
{
  return new Fl_Emscripten_Gl_Window_Driver(w);
}

void Fl_Emscripten_Gl_Window_Driver::draw_string_legacy(const char* str, int n) {
  draw_string_legacy_get_list(str, n);
}

int Fl_Emscripten_Gl_Window_Driver::genlistsize() {
  return 256;
}

void Fl_Emscripten_Gl_Window_Driver::gl_bitmap_font(Fl_Font_Descriptor *fl_fontsize) {
  if (!fl_fontsize->listbase) {
    fl_fontsize->listbase = glGenLists(genlistsize());
  }
  glListBase(fl_fontsize->listbase);
}

void Fl_Emscripten_Gl_Window_Driver::get_list(Fl_Font_Descriptor *fd, int r) {
  (void)fd; (void)r;
}

Fl_Font_Descriptor** Fl_Emscripten_Gl_Window_Driver::fontnum_to_fontdescriptor(int fnum) {
  Fl_Fontdesc *s = fl_fonts + fnum;
  return &(s->first);
}

Fl_Gl_Choice *Fl_Emscripten_Gl_Window_Driver::find(int m, const int *alistp)
{
  Fl_Emscripten_Gl_Choice *g = (Fl_Emscripten_Gl_Choice*)Fl_Gl_Window_Driver::find_begin(m, alistp);
  if (g) return g;

  g = new Fl_Emscripten_Gl_Choice(m, alistp, first);
  first = g;
  return g;
}

GLContext Fl_Emscripten_Gl_Window_Driver::create_gl_context(Fl_Window* window,
                                                            const Fl_Gl_Choice* g) {
  (void)window;
  EmscriptenWebGLContextAttributes attrs;
  emscripten_webgl_init_context_attributes(&attrs);

  // Translate FLTK flags to EmscriptenWebGLContextAttributes
  int m = g->mode();
  attrs.alpha = (m & FL_ALPHA) ? 1 : 0;
  attrs.depth = ((m & FL_DEPTH) || (m & FL_DEPTH32)) ? 1 : 0;
  attrs.stencil = (m & FL_STENCIL) ? 1 : 0;
  attrs.antialias = (m & FL_MULTISAMPLE) ? 1 : 0;
  attrs.enableExtensionsByDefault = 1;

  // Attempt WebGL 2 first, falling back to WebGL 1 if unavailable
  attrs.majorVersion = 2;
  attrs.minorVersion = 0;

  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context(0, &attrs);
  if (!ctx && attrs.majorVersion == 2) {
    attrs.majorVersion = 1;
    ctx = emscripten_webgl_create_context(0, &attrs);
  }

  if (ctx) {
    add_context((GLContext)ctx);
  }
  return (GLContext)ctx;
}

void Fl_Emscripten_Gl_Window_Driver::set_gl_context(Fl_Window* w, GLContext context) {
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE current_context = emscripten_webgl_get_current_context();
  if ((EMSCRIPTEN_WEBGL_CONTEXT_HANDLE)context != current_context || w != cached_window) {
    cached_window = w;
    emscripten_webgl_make_context_current((EMSCRIPTEN_WEBGL_CONTEXT_HANDLE)context);
  }
}

void Fl_Emscripten_Gl_Window_Driver::delete_gl_context(GLContext context) {
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE current_context = emscripten_webgl_get_current_context();
  if (current_context == (EMSCRIPTEN_WEBGL_CONTEXT_HANDLE)context) {
    cached_window = 0;
    emscripten_webgl_make_context_current(0);
  }
  emscripten_webgl_destroy_context((EMSCRIPTEN_WEBGL_CONTEXT_HANDLE)context);
  del_context(context);
}

void Fl_Emscripten_Gl_Window_Driver::make_overlay_current() {
    glDrawBuffer(GL_FRONT);
}

void Fl_Emscripten_Gl_Window_Driver::redraw_overlay() {
    pWindow->damage(FL_DAMAGE_OVERLAY);
}

void Fl_Emscripten_Gl_Window_Driver::before_show(int&) {
  Fl_X::make_xid(pWindow, 0, 0);
}

float Fl_Emscripten_Gl_Window_Driver::pixels_per_unit()
{
  int ns = pWindow->screen_num();
  return Fl::screen_driver()->scale(ns);
}

int Fl_Emscripten_Gl_Window_Driver::mode_(int m, const int *a) {
  mode(m); alist(a);
  if (pWindow->shown()) {
    g( find(m, a) );
    pWindow->hide();
    pWindow->show();
  } else {
    g(0);
  }
  return 1;
}

void Fl_Emscripten_Gl_Window_Driver::swap_buffers() {
  if (!fl_xid(pWindow))
    return;
  // Emscripten handles buffer swapping on the canvas automatically per frame render.
}

char Fl_Emscripten_Gl_Window_Driver::swap_type() {
  return copy;
}

void Fl_Emscripten_Gl_Window_Driver::swap_interval(int interval) {
  (void)interval;
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
