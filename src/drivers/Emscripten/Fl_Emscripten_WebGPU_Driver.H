//
// Class Fl_Emscripten_WebGPU_Window_Driver for the Fast Light Tool Kit (FLTK).
//
// Copyright 2022-2025 by Bill Spitzak and others.
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

#ifndef FL_EMSCRIPTEN_WEBGPU_WINDOW_DRIVER_H
#define FL_EMSCRIPTEN_WEBGPU_WINDOW_DRIVER_H

#include <config.h>
#if HAVE_GL
#include <FL/platform.H>
#include "../../Fl_WebGPU_Window_Driver.H"

class Fl_WebGPU_Choice;

class Fl_Emscripten_WebGPU_Window_Driver : public Fl_WebGPU_Window_Driver {
  friend Fl_WebGPU_Window_Driver* Fl_WebGPU_Window_Driver::newWebGPUWindowDriver(Fl_WebGPU_Window *);
  Fl_Emscripten_WebGPU_Window_Driver(Fl_WebGPU_Window *win) : Fl_WebGPU_Window_Driver(win) {}
  float pixels_per_unit() FL_OVERRIDE;
  void before_show(int& need_after) FL_OVERRIDE;
  int mode_(int m, const int *a) FL_OVERRIDE;
  void swap_buffers() FL_OVERRIDE;
  char swap_type() FL_OVERRIDE;
  void swap_interval(int) FL_OVERRIDE;
  int swap_interval() const FL_OVERRIDE;
  Fl_WebGPU_Choice *find(int m, const int *alistp) FL_OVERRIDE;
  void make_overlay_current() FL_OVERRIDE;
  void redraw_overlay() FL_OVERRIDE;
};


#endif // HAVE_WEBGPU

#endif // FL_EMSCRIPTEN_WEBGPU_WINDOW_DRIVER_H
