//
// Definition of Emscripten Browser Screen interface
// for the Fast Light Tool Kit (FLTK).
//
// Copyright 2010-2024 by Bill Spitzak and others.
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

#ifndef FL_EMSCRIPTEN_SCREEN_DRIVER_H
#define FL_EMSCRIPTEN_SCREEN_DRIVER_H

#include "../../Fl_Screen_Driver.H"
#include <FL/fl_attr.h>

class Fl_Emscripten_Screen_Driver : public Fl_Screen_Driver {
public:
  void init() FL_OVERRIDE;
  Fl_Emscripten_Screen_Driver();
  void screen_work_area(int &X, int &Y, int &W, int &H, int n) FL_OVERRIDE;
  void screen_xywh(int &X, int &Y, int &W, int &H, int n) FL_OVERRIDE;
  int x() FL_OVERRIDE;
  int y() FL_OVERRIDE;
  int w() FL_OVERRIDE;
  int h() FL_OVERRIDE;
  void grab(Fl_Window *win) FL_OVERRIDE;
  int get_mouse(int &x, int &y) FL_OVERRIDE;
  int compose(int &del) FL_OVERRIDE;
  void copy(const char *stuff, int len, int clipboard, const char *type) FL_OVERRIDE;
  void paste(Fl_Widget &receiver, int clipboard, const char *type) FL_OVERRIDE;
};

#endif