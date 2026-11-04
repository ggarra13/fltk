//
// Copy-to-clipboard code for the Fast Light Tool Kit (FLTK).
//
// Copyright 2024 by Bill Spitzak and others.
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

#ifndef FL_EMSCRIPTEN_COPY_SURFACE_DRIVER_H
#define FL_EMSCRIPTEN_COPY_SURFACE_DRIVER_H

#include <FL/Fl_Copy_Surface.H>
#include <FL/Fl_Image_Surface.H>
#include <FL/platform.H>

class Fl_Emscripten_Copy_Surface_Driver : public Fl_Copy_Surface_Driver {
  friend class Fl_Copy_Surface_Driver;
  Fl_Image_Surface *img_surf;

protected:
  Fl_Emscripten_Copy_Surface_Driver(int w, int h);
  ~Fl_Emscripten_Copy_Surface_Driver();
  void set_current() FL_OVERRIDE;
  void translate(int x, int y) FL_OVERRIDE;
  void untranslate() FL_OVERRIDE;
};

#endif