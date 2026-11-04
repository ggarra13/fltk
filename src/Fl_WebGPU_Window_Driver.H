//
// Definition of class Fl_WebGPU_Window_Driver, and of its platform-specific derived classes
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

#ifndef Fl_WebGPU_Window_Driver_H
#define Fl_WebGPU_Window_Driver_H

#include <FL/Fl_WebGPU_Window.H>

class Fl_WebGPU_Choice;

/* The constructor of each Fl_WebGPU_Window object creates also an object from a
   platform-specific derived class from this class.
*/
class Fl_WebGPU_Window_Driver {
protected:
  Fl_WebGPU_Window *pWindow;

public:
  static Fl_Window *cached_window;
  static Fl_WebGPU_Choice *first;
  Fl_WebGPU_Choice *g() { return pWindow->g; }
  void g(Fl_WebGPU_Choice *c) { pWindow->g = c; }
  int mode() { return pWindow->mode_; }
  void mode(int m) { pWindow->mode_ = m; }
  const int *alist() { return pWindow->alist; }
  void alist(const int *l) { pWindow->alist = l; }

  Fl_WebGPU_Window_Driver(Fl_WebGPU_Window *win);
  virtual ~Fl_WebGPU_Window_Driver();
  static Fl_WebGPU_Window_Driver *newWebGPUWindowDriver(Fl_WebGPU_Window *w);
  static Fl_WebGPU_Window_Driver *global();
  virtual int get_surface_buffer_scale() const { return 1; }
  virtual float pixels_per_unit() { return 1; }
  virtual void before_show(int &) {}
  virtual void after_show() {}
  virtual void swap_interval(int x);
  virtual int swap_interval() const {return swap_interval_;};
  virtual int flush_begin() { return 0; };  // return 1 to skip drawing
  virtual void invalidate();
  virtual int mode_(int /*m*/, const int * /*a*/) { return 0; }
  virtual void make_current_before() {}
  virtual void make_current_after() {}
  virtual void swap_buffers() {};
  virtual void resize(int is_a_resize, int w, int h);
  virtual void prepare();

  static Fl_WebGPU_Choice *find_begin(int m, const int *alistp);
  // Return one of these structures for a given WebGPU mode.
  virtual Fl_WebGPU_Choice *find(int /*mode*/, const int * /*alistp*/) { return NULL; }
  static inline Fl_WebGPU_Window_Driver *driver(const Fl_WebGPU_Window *win) {
    return win->pWebGPUWindowDriver;
  }

  // Platform specific: must create pWindow->wgpu.surface from the native
  // window handle (X11, Wayland, Win32, Cocoa...). Needs wgpu.instance.
  virtual void create_surface() = 0;
  virtual void destroy_surface();
  virtual void destroy_resources();
  virtual bool has_hdr_monitor() { return true; }

  // Called in this order by Fl_WebGPU_Window::init_webgpu()
  void init_instance();     // wgpu.instance
  void create_device();     // wgpu.adapter, wgpu.device, wgpu.queue()
  void init_colorspace();   // wgpu.surface_format and present mode

  // Releases queue, device, adapter and instance (after destroy_surface()).
  void destroy_device();

  // Per-frame: get the surface texture to render to / release it again.
  // acquire_back_buffer() returns false if the frame must be skipped.
  bool acquire_back_buffer();
  void release_back_buffer();

protected:
  void prepare_buffers();     // (re)configures the surface (swapchain)
  void prepare_depth();       // (re)creates the depth/stencil texture
  void destroy_depth();
  void apply_present_mode();

private:
  int swap_interval_ = 0;
  WGPUPresentMode present_mode_ = WGPUPresentMode_Fifo;
  bool needs_configure_ = false;
  int configured_w_ = 0;
  int configured_h_ = 0;
};

#endif /* Fl_WebGPU_Window_Driver_H */

/**
 \}
 \endcond
 */
