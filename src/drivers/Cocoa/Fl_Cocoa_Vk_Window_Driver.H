//
// Class Fl_Cocoa_Vulkan_Window_Driver for the Fast Light Tool Kit (FLTK).
//
// Copyright 2021-2023 by Bill Spitzak and others.
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

#ifndef FL_WINAPI_VULKAN_WINDOW_DRIVER_H
#define FL_WINAPI_VULKAN_WINDOW_DRIVER_H

#include <config.h>
#if HAVE_VK
#include <FL/platform.H>
#include "../../Fl_Vk_Window_Driver.H"
#include <FL/vk.h>

class Fl_Cocoa_Vk_Window_Driver : public Fl_Vk_Window_Driver {
  friend Fl_Vk_Window_Driver* Fl_Vk_Window_Driver::newVkWindowDriver(Fl_Vk_Window *);
  Fl_Cocoa_Vk_Window_Driver(Fl_Vk_Window *win) : Fl_Vk_Window_Driver(win) {};
  virtual ~Fl_Cocoa_Vk_Window_Driver() {};
  float pixels_per_unit() FL_OVERRIDE;
  int mode_(int m, const int *a) FL_OVERRIDE;
  // void make_current_after() FL_OVERRIDE;
  void swap_interval(int) FL_OVERRIDE;
  int swap_interval() const FL_OVERRIDE;
  void invalidate() FL_OVERRIDE {}
  // int flush_begin() FL_OVERRIDE;
  Fl_Vk_Choice *find(int m, const int *alistp) FL_OVERRIDE;
  // void get_list(Fl_Font_Descriptor *fd, int r) FL_OVERRIDE;
  // int genlistsize() FL_OVERRIDE;
  void* GetProcAddress(const char *procName) FL_OVERRIDE;
#if !(USE_XFT || FLTK_USE_CAIRO)
  // virtual Fl_Font_Descriptor** fontnum_to_fontdescriptor(int fnum) FL_OVERRIDE;
#endif
  std::vector<std::string> get_required_extensions() FL_OVERRIDE;
  void create_surface() FL_OVERRIDE;
};

#endif // HAVE_VK

#endif // FL_WINAPI_VULKAN_WINDOW_DRIVER_H
