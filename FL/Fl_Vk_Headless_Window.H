//
// Fl_Vk_Headless_Window -- CLI / offscreen Vulkan rendering for FLTK.
//
// A drop-in Fl_Vk_Window that is never shown() on screen. Install it,
// override draw() as usual, and call render_offscreen() to render a frame,
// then capture_vk_rectangle() to read the pixels back -- no Fl::run(), no
// display connection, no native window required.
//
//   Fl_Vk_Headless_Window win(1920, 1080);
//   win.render_offscreen();
//   Fl_RGB_Image *img = win.capture_vk_rectangle(0, 0, win.pixel_w(), win.pixel_h());
//   img->... // save/encode as needed
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

#ifndef Fl_Vk_Headless_Window_H
#define Fl_Vk_Headless_Window_H

#include <FL/Fl_Vk_Window.H>

class Fl_Vk_Window_Driver;

class FL_EXPORT Fl_Vk_Headless_Window : public Fl_Vk_Window {
protected:
  // Installs Fl_Vk_Headless_Window_Driver instead of going through
  // Fl_Vk_Window_Driver::newVkWindowDriver()'s per-platform (X11/Wayland/
  // Win32/Cocoa) dispatch -- none of those apply here, since this window is
  // never mapped on screen.
  Fl_Vk_Window_Driver *create_driver() FL_OVERRIDE;

  std::vector<const char*> get_optional_extensions() { return {}; };
public:
  Fl_Vk_Headless_Window(int W, int H, const char *l = 0) : Fl_Vk_Window(W, H, l) {}
};

#endif // Fl_Vk_Headless_Window_H
