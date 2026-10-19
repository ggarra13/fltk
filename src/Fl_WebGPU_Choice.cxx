//
// OpenGL visual selection code for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2021 by Bill Spitzak and others.
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

/**
 \cond DriverDev
 \addtogroup DriverDeveloper
 \{
 */
#include <FL/Fl.H>
#include "Fl_WebGPU_Choice.H"
#include <FL/Fl_WebGPU_Window.H>
#include "Fl_WebGPU_Window_Driver.H"
#include <stdlib.h>


Fl_WebGPU_Choice *Fl_WebGPU_Window_Driver::first;

// this assumes one of the two arguments is zero:
// We keep the list system in Win32 to stay compatible and interpret
// the list later...
Fl_WebGPU_Choice *Fl_WebGPU_Window_Driver::find_begin(int m, const int *alistp) {
  Fl_WebGPU_Choice *g;
  for (g = first; g; g = g->next)
    if (g->mode == m && g->alist == alistp)
      return g;
  return NULL;
}

/**
 \}
 \endcond
 */

#endif // HAVE_GL
