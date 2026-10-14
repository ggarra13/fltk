//
// Definition of Emscripten system driver used for the web.
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

#ifndef FL_EMSCRIPTEN_SYSTEM_DRIVER_H
#define FL_EMSCRIPTEN_SYSTEM_DRIVER_H

#include "../../Fl_System_Driver.H"

class Fl_Emscripten_System_Driver : public Fl_System_Driver {

public:
  Fl_Emscripten_System_Driver();
  double wait(double) FL_OVERRIDE;
  char *strdup(const char *) FL_OVERRIDE;
};

#endif