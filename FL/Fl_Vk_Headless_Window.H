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

//! Pure convenience: equivalent to `Fl_Vk_Window win(W, H, l); win.headless(true);`
//! Fl_Vk_Window itself now understands headless() as a runtime flag (see
//! its create_driver()/get_optional_extensions()), so this class exists
//! only for code that prefers a distinct, compile-time-enforced type.
//! Prefer calling headless(true) directly on your own Fl_Vk_Window
//! subclass if you want the *same* class to serve both an interactive UI
//! and a CLI/offscreen workflow, chosen at runtime -- that's the whole
//! point of the flag living on the base class.
class FL_EXPORT Fl_Vk_Headless_Window : public Fl_Vk_Window {
public:
  Fl_Vk_Headless_Window(int W, int H, const char *l = 0) : Fl_Vk_Window(W, H, l) {
    headless(true);
  }
  Fl_Vk_Headless_Window(int X, int Y, int W, int H, const char *l = 0) : Fl_Vk_Window(X, Y, W, H, l) {
    headless(true);
  }
};

#endif // Fl_Vk_Headless_Window_H
