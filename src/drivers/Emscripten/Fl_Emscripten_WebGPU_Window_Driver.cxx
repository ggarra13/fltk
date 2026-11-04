//
// Class Fl_Emscripten_WebGPU_Window_Driver for the Fast Light Tool Kit (FLTK).
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
#if HAVE_WEBGPU
#include <stdio.h>
#include <FL/platform.H>
#include <FL/Fl_WebGPU_Window.H>
#include "../../Fl_WebGPU_Choice.H"
#include "../../Fl_Screen_Driver.H"
#include "Fl_Emscripten_WebGPU_Window_Driver.H"
#include <emscripten/html5.h>
#include <emscripten/emscripten.h>

// Describes context choices and holds translated WebGL attributes.
class Fl_Emscripten_WebGPU_Choice : public Fl_WebGPU_Choice {
  friend class Fl_Emscripten_WebGPU_Window_Driver;
private:
public:
  Fl_Emscripten_WebGPU_Choice(int m, const int *alistp, Fl_WebGPU_Choice *n)
    : Fl_WebGPU_Choice(m, alistp, n) {
  }
};

static void fl_make_canvas_target(Fl_Window *window, char *buf, size_t len) {
  fl_uintptr_t xid = fl_xid(window);
  snprintf(buf, len, "#fltk_canvas%lu", (unsigned long)xid);
}

Fl_WebGPU_Window_Driver *Fl_WebGPU_Window_Driver::newWebGPUWindowDriver(Fl_WebGPU_Window *w)
{
  return new Fl_Emscripten_WebGPU_Window_Driver(w);
}

// Used by Fl_WebGPU_Window::can_do(), which has no window instance.
Fl_WebGPU_Window_Driver *Fl_WebGPU_Window_Driver::global()
{
  static Fl_WebGPU_Window_Driver *gd = new Fl_Emscripten_WebGPU_Window_Driver(NULL);
  return gd;
}

Fl_WebGPU_Choice *Fl_Emscripten_WebGPU_Window_Driver::find(int m, const int *alistp)
{
  // Reject what WebGPU cannot provide so Fl_WebGPU_Window::can_do() is truthful.
  // NOTE: when an alist is used, m may be 0 here; alist parsing is not done.
  if (m & (FL_STEREO | FL_ACCUM)) return 0;

  Fl_WebGPU_Choice *g = Fl_Emscripten_WebGPU_Window_Driver::find_begin(m, alistp);
  if (g) return g;

  g = new Fl_Emscripten_WebGPU_Choice(m, alistp, first);
  first = g;
  return g;
}

void Fl_Emscripten_WebGPU_Window_Driver::before_show(int&) {
}

// Creates the WGPUSurface from the window's <canvas> element
// (id "fltk_canvas<xid>", created by Fl_Emscripten_Window_Driver::makeWindow()).
// Called from Fl_WebGPU_Window::init_webgpu() after the window is shown, so the
// canvas already exists, and after wgpu.instance has been created.
//
// IMPORTANT: a canvas that has ever returned a "2d" context can never give a
// WebGPU context. Fl_Emscripten_Window_Driver::make_current() must not run for
// WebGPU windows (see the guard suggested there).
void Fl_Emscripten_WebGPU_Window_Driver::create_surface() {
  Fl_WebGPU_Context &ctx = pWindow->wgpu;
  if (ctx.surface)
    return;
  if (!ctx.instance) {
    fprintf(stderr, "create_surface: no WebGPU instance\n");
    return;
  }

  char selector[64];
  fl_make_canvas_target(pWindow, selector, sizeof(selector));

  WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas_src = {};
  canvas_src.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
  canvas_src.selector    = WGPUStringView{selector, WGPU_STRLEN};

  WGPUSurfaceDescriptor desc = {};
  desc.nextInChain = &canvas_src.chain;
  desc.label       = WGPUStringView{"FLTK canvas surface", WGPU_STRLEN};

  ctx.surface = wgpuInstanceCreateSurface(ctx.instance, &desc);
  if (!ctx.surface)
    fprintf(stderr, "create_surface: could not create surface for %s\n", selector);
}


float Fl_Emscripten_WebGPU_Window_Driver::pixels_per_unit()
{
  int ns = pWindow->screen_num();
  return Fl::screen_driver()->scale(ns);
}

int Fl_Emscripten_WebGPU_Window_Driver::mode_(int m, const int *a) {
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

void Fl_Emscripten_WebGPU_Window_Driver::swap_buffers() {
  // WebGPU presents implicitly when control returns to the browser.
  // (Only call emscripten_webgl_commit_frame() if explicitSwapControl is enabled.)
}

void Fl_Emscripten_WebGPU_Window_Driver::swap_interval(int interval) {
  (void)interval; // tied to requestAnimationFrame
}

int Fl_Emscripten_WebGPU_Window_Driver::swap_interval() const {
  return -1;
}

#endif // HAVE_WEBGPU
