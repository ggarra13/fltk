//
// Emscripten-specific printing support for the Fast Light Tool Kit (FLTK).
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

#include <FL/Fl_Paged_Device.H>
#include <FL/Fl_Printer.H>
#include "Fl_Emscripten_Graphics_Driver.H"

/** A stub to allow compilation. Printing is supported by browsers already.
Adding such functionality will just require a `window.print()` which has the same effect as printing
from the browser directly!
 */
class Fl_Emscripten_Printer_Driver : public Fl_Paged_Device {
  friend class Fl_Printer;

protected:
  Fl_Emscripten_Printer_Driver(void);
};

Fl_Emscripten_Printer_Driver::Fl_Emscripten_Printer_Driver(void)
  : Fl_Paged_Device() {
  driver(new Fl_Emscripten_Graphics_Driver);
}

Fl_Paged_Device *Fl_Printer::newPrinterDriver(void) {
  return new Fl_Emscripten_Printer_Driver();
}