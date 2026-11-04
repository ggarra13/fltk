//
// WebGPU definitions for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2018 by Bill Spitzak and others.
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

// Internal interface to set up WebGPU.
//
// A "Fl_WebGPU_Choice" is created from an WebGPU mode and holds information
// necessary to create a window (on X) and to create an WebGPU "context"
// (on both X and Win32).
//
// This code is used by Fl_WebGPU_Window

#ifndef Fl_WebGPU_Choice_H
#define Fl_WebGPU_Choice_H

// Describes the platform-independent part of data needed to create a GLContext.
class Fl_WebGPU_Choice {
  friend class Fl_WebGPU_Window_Driver;
  int mode;
  const int *alist;
  Fl_WebGPU_Choice *next;
public:
  Fl_WebGPU_Choice(int m, const int *alistp, Fl_WebGPU_Choice *n) : mode(m), alist(alistp), next(n) {}
};

#endif // Fl_WebGPU_Choice_H
