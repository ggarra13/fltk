//
// Definition of class Fl_Vk_Window_Driver, and of its platform-specific derived classes
// for the Fast Light Tool Kit (FLTK).
//
// Copyright 2016-2018 by Bill Spitzak and others.
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

/**
 \cond DriverDev
 \addtogroup DriverDeveloper
 \{
 */

#ifndef Fl_Vk_Window_Driver_H
#define Fl_Vk_Window_Driver_H

#include <string>
#include <vector>

#include <FL/Fl_Vk_Window.H>
#include <FL/vk.h>

class Fl_Vk_Choice;
class Fl_Font_Descriptor;

/* The constructor of each Fl_Vk_Window object creates also an object from a
   platform-specific derived class from this class.
*/
class Fl_Vk_Window_Driver {
protected:
  Fl_Vk_Window *pWindow;

public:
  static Fl_Window *cached_window;
  static Fl_Vk_Choice *first;
  static int copy;
  static float vk_scale;
  Fl_Vk_Choice *g() { return pWindow->g; }
  void g(Fl_Vk_Choice *c) { pWindow->g = c; }
  int mode() { return pWindow->mode_; }
  void mode(int m) { pWindow->mode_ = m; }
  const int *alist() { return pWindow->alist; }
  void alist(const int *l) { pWindow->alist = l; }

  Fl_Vk_Window_Driver(Fl_Vk_Window *win)
    : pWindow(win) {}
  virtual ~Fl_Vk_Window_Driver() {}
  static Fl_Vk_Window_Driver *newVkWindowDriver(Fl_Vk_Window *w);
  static Fl_Vk_Window_Driver *global();
  virtual float pixels_per_unit() { return 1; }
  virtual void before_show(int &) {}
  virtual void after_show() {}
  virtual void swap_interval(int) {};
  virtual int swap_interval() const {return 1;};
  virtual int flush_begin() { return 0; };
  virtual void invalidate();
  virtual int mode_(int /*m*/, const int * /*a*/) { return 0; }
  virtual void make_current_before() {}
  virtual void make_current_after() {}
  virtual void swap_buffers() {};
  virtual void prepare();
  virtual void vk_hide_before(void *&) {} // the default implementation may be enough
  static Fl_Vk_Choice *find_begin(int m, const int *alistp);
  // Return one of these structures for a given vk mode.
  virtual Fl_Vk_Choice *find(int /*mode*/, const int * /*alistp*/) { return NULL; }
  virtual void *GetProcAddress(const char *procName) { return nullptr; }
  virtual void get_list(Fl_Font_Descriptor *, int) {} // support for gl_draw() without textures
  static void draw_string_with_texture(const char *str, int n); // cross-platform
  // support for vk_draw(). The cross-platform version may be enough.
  virtual char *alpha_mask_for_string(const char *str, int n, int w, int h, Fl_Fontsize fs);
  virtual int genlistsize() { return 0; } // support for gl_draw()
  virtual Fl_Font_Descriptor **fontnum_to_fontdescriptor(int fnum);
  virtual Fl_RGB_Image *capture_vk_rectangle(int x, int y, int w, int h);
  static inline Fl_Vk_Window_Driver *driver(const Fl_Vk_Window *win) {
    return win->pVkWindowDriver;
  }

  // true means the platform uses vkScissor() to make sure VK subwindows
  // don't leak outside their parent window
  virtual bool need_scissor() { return false; }

  // Vulkan specific methods.
  virtual std::vector<const char*> get_instance_extensions() = 0;
  virtual void create_surface() = 0;
  virtual void destroy_surface();
  virtual void destroy_resources();

  void init_colorspace();
  void init_vk();

protected:
  void init_instance();
  void prepare_buffers();
  void prepare_depth();
  void prepare_framebuffers();
};

#endif /* Fl_Vk_Window_Driver_H */

/**
 \}
 \endcond
 */
