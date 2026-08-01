//
// Class Fl_Vk_Headless_Window for the Fast Light Tool Kit (FLTK).
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
#if HAVE_VK

#include <FL/Fl_Vk_Headless_Window.H>
#include "Fl_Vk_Headless_Window_Driver.H"
#include <iostream>

Fl_Vk_Window_Driver *Fl_Vk_Headless_Window::create_driver() {
  return new Fl_Vk_Headless_Window_Driver(this);
}

#endif // HAVE_VK
