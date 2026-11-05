//
// Emscripten-specific code to initialize browser support.
//
// Copyright 2022-2024 by Bill Spitzak and others.
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

#include "Fl_Emscripten_Screen_Driver.H"
#include "Fl_Emscripten_System_Driver.H"
#include "Fl_Emscripten_Image_Surface_Driver.H"
#include "Fl_Emscripten_Graphics_Driver.H"
#include "Fl_Emscripten_Window_Driver.H"
#include "Fl_Emscripten_Copy_Surface_Driver.H"
#include <FL/Fl_Native_File_Chooser.H>

Fl_Screen_Driver *Fl_Screen_Driver::newScreenDriver() {
  return new Fl_Emscripten_Screen_Driver();
}

Fl_System_Driver *Fl_System_Driver::newSystemDriver() {
  return new Fl_Emscripten_System_Driver();
}

Fl_Image_Surface_Driver *Fl_Image_Surface_Driver::newImageSurfaceDriver(int w, int h, int highres,
                                                                        Fl_Offscreen off) {
  return new Fl_Emscripten_Image_Surface_Driver(w, h, highres, off);
}

Fl_Graphics_Driver *Fl_Graphics_Driver::newMainGraphicsDriver() {
  return new Fl_Emscripten_Graphics_Driver();
}

Fl_Window_Driver *Fl_Window_Driver::newWindowDriver(Fl_Window *w) {
  return new Fl_Emscripten_Window_Driver(w);
}

Fl_Copy_Surface_Driver *Fl_Copy_Surface_Driver::newCopySurfaceDriver(int w, int h) {
  return new Fl_Emscripten_Copy_Surface_Driver(w, h);
}
