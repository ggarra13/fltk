//
// Definition of class Fl_Vk_Headless_Window_Driver for the Fast Light Tool
// Kit (FLTK).
//
// This driver enables CLI / offscreen use of Fl_Vk_Window: it never touches
// window-system integration (no VkSurfaceKHR, no swapchain, no native
// window), and renders into plain VkImages managed directly by
// Fl_Vk_Window_Driver::prepare_offscreen_buffers().
//
// Unlike the X11/Wayland/Win32/Cocoa drivers, this one is entirely
// platform-independent, so there is exactly one of it (no per-platform
// variants, and it is never selected by
// Fl_Vk_Window_Driver::newVkWindowDriver()'s platform dispatch -- see
// Fl_Vk_Headless_Window::create_driver() in FL/Fl_Vk_Headless_Window.H).
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

#ifndef Fl_Vk_Headless_Window_Driver_H
#define Fl_Vk_Headless_Window_Driver_H

#include "Fl_Vk_Window_Driver.H"

class Fl_Vk_Headless_Window_Driver : public Fl_Vk_Window_Driver {
public:
  Fl_Vk_Headless_Window_Driver(Fl_Vk_Window *win) : Fl_Vk_Window_Driver(win) {}
  virtual ~Fl_Vk_Headless_Window_Driver() {}

  bool is_headless() const FL_OVERRIDE { return true; }

  // No VK_KHR_surface / platform-surface extension is needed: this driver
  // never creates a VkSurfaceKHR.
  std::vector<const char*> get_instance_extensions() FL_OVERRIDE { return {}; }

  // Intentionally does nothing: pWindow->m_surface stays VK_NULL_HANDLE.
  // Fl_Vk_Window::init_vulkan() and Fl_Vk_Window::swap_buffers() both check
  // is_headless()/m_surface and know a null surface here is expected, not
  // an error.
  void create_surface() FL_OVERRIDE {}

  // There is no monitor at all, so no HDR monitor either.
  bool has_hdr_monitor() FL_OVERRIDE { return false; }

  // The interactive per-platform drivers (X11/Wayland/etc.) that override
  // this typically do an OS-level screen grab, which doesn't make sense
  // here since there is no on-screen window. Reads back directly from the
  // current offscreen VkImage instead.
  Fl_RGB_Image *capture_vk_rectangle(int x, int y, int w, int h) FL_OVERRIDE;
};

#endif // Fl_Vk_Headless_Window_Driver_H
