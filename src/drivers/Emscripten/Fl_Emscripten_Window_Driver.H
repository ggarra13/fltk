//
// Definition of the Emscripten window driver.
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

#ifndef FL_EMSCRIPTEN_WINDOW_DRIVER_H
#define FL_EMSCRIPTEN_WINDOW_DRIVER_H

#include "../../Fl_Window_Driver.H"
#include <FL/fl_attr.h>

class Fl_Emscripten_Window_Driver : public Fl_Window_Driver {
  const char *cursor;

public:
  Fl_Emscripten_Window_Driver(Fl_Window *);
  void makeWindow() FL_OVERRIDE;
  void show() FL_OVERRIDE;
  void map() FL_OVERRIDE;
  void hide() FL_OVERRIDE;
  void unmap() FL_OVERRIDE;
  void make_current() FL_OVERRIDE;
  void use_border() FL_OVERRIDE;
  void take_focus() FL_OVERRIDE;
  int decorated_w() FL_OVERRIDE { return pWindow->w(); }
  int decorated_h() FL_OVERRIDE { return pWindow->h() + 30; }
  void fullscreen_on() FL_OVERRIDE;
  void fullscreen_off(int /*X*/, int /*Y*/, int /*W*/, int /*H*/) FL_OVERRIDE;
  void resize(int /*X*/, int /*Y*/, int /*W*/, int /*H*/) FL_OVERRIDE;
  int set_cursor(Fl_Cursor c) FL_OVERRIDE;
  void flush() FL_OVERRIDE;
};

#endif