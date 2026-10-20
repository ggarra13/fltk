//
// WebGPU header file for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2026 by Bill Spitzak and others.
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

/* \file
   Fl_WebGPU_Window widget . */

#ifndef Fl_WebGPU_Window_H
#define Fl_WebGPU_Window_H

#include "Fl_Window.H"
#include "Fl_WebGPU_Context.H"

class Fl_WebGPU_Choice;
class Fl_WebGPU_Window_Driver;

/**
  The Fl_WebGPU_Window widget sets things up so WebGPU works.

  Please note that the FLTK drawing and clipping functions
  will not work inside an Fl_WebGPU_Window. All drawing
  should be done using WebGPU calls exclusively.
*/
class FL_EXPORT Fl_WebGPU_Window : public Fl_Window {
  friend class Fl_WebGPU_Window_Driver;
  Fl_WebGPU_Window_Driver *pWebGPUWindowDriver;

  int mode_;
  const int *alist;
  Fl_WebGPU_Choice *g;
  char valid_f_;
  char damage1_; // damage() of back buffer
  virtual void draw_overlay();
  void init();

  static int can_do(int, const int *);
  int mode(int, const int *);

  Fl_WebGPU_Window_Driver* create_driver();

protected:
  //! Tracking variable to know if begin_render_pass()/end_render_pass() has
  //! been called.
  bool                   m_in_render_pass = false;
  bool                   m_frame_active = false;

  bool m_debugSync = false;

  void init_webgpu();
  void shutdown_webgpu();

  virtual void init_colorspace();

  void draw_begin();
  void draw() override;
  void draw_end();

  WGPUColor clearValues = { 0.F, 0.F, 0.F, 1.F };

  // Auxiliary functions to begin and end the main render pass.
  // Note that FLTK will call begin_render_pass() in draw_begin()
  // and end_render_pass() in draw_end().
  // That is: draw() is called with an active render pass by default.
  // That will clear the canvas to the clearValus color and depth/stencil.
  void begin_render_pass();
  void end_render_pass();

public:
  void show() override;
  /** Same as Fl_Window::show(int a, char **b) */
  void show(int a, char **b) {Fl_Window::show(a,b);}
  void flush() override;
  void hide() override;
  void resize(int,int,int,int) override;
  int handle(int) override;

  // Window's WebGPU context.
  Fl_WebGPU_Context wgpu;

  inline WGPUQueue&        queue() { return wgpu.safe_thread_queue.queue; }
  inline std::mutex& queue_mutex() { return wgpu.safe_thread_queue.mutex; }

  /**
    Is turned off when FLTK creates a new context for this window or
    when the window resizes, and is turned on \e after draw() is called.
    You can use this inside your draw() method to avoid unnecessarily
    initializing the WebGPU context. Just do this:
    \code
    void mywindow::draw() {
     if (!valid()) {
       ...other initialization...
     }
     ... draw your geometry here ...
    }
    \endcode

    You can turn valid() on by calling valid(1).  You
    should only do this after fixing the transformation inside a draw()
    or after make_current().  This is done automatically after
    draw() returns.
  */
  char valid() const {return valid_f_ & 1;}
  /**
    See char Fl_WebGPU_Window::valid() const
  */
  void valid(char v) {if (v) valid_f_ |= 1; else valid_f_ &= 0xfe;}
  void invalidate();

  /**
    Will only be set if the
    WebGPU context is created or recreated. It differs from
    Fl_WebGPU_Window::valid() which is also set whenever the context
    changes size.
  */
  char context_valid() const {return valid_f_ & 2;}
  /**
    See char Fl_WebGPU_Window::context_valid() const
  */
  void context_valid(char v) {if (v) valid_f_ |= 2; else valid_f_ &= 0xfd;}

  /**  Returns non-zero if the hardware supports the given WebGPU mode. */
  static int can_do(int m) {return can_do(m,0);}
  /**  Returns non-zero if the hardware supports the given WebGPU mode.
   \see Fl_WebGPU_Window::mode(const int *a) */
  static int can_do(const int *m) {return can_do(0, m);}
  /**  Returns non-zero if the hardware supports the current WebGPU mode. */
  int can_do() const { return can_do(mode_,alist); }
  /** Returns the current WebGPU capabilities of the window.
   Don't use this if capabilities were set through Fl_WebGPU_Window::mode(const int *a).
   */
  Fl_Mode mode() const {return (Fl_Mode)mode_;}
  /**
   Set or change the WebGPU capabilities of the window.  The value can be
   any of the following OR'd together:

   - \c FL_RGB - RGB color (not indexed)
   - \c FL_RGB8 - RGB color with at least 8 bits of each color
   - \c FL_INDEX - Indexed mode
   - \c FL_SINGLE - not double buffered
   - \c FL_DOUBLE - double buffered
   - \c FL_ACCUM - accumulation buffer
   - \c FL_ALPHA - alpha channel in color
   - \c FL_DEPTH - any depth buffer (or set FL_DEPTH32 for at least 32 bits)
   - \c FL_DEPTH32 - depth buffer with at least 32 bits
   - \c FL_STENCIL - stencil buffer
   - \c FL_MULTISAMPLE - multisample antialiasing

   FL_RGB and FL_SINGLE have a value of zero, so they
   are "on" unless you give FL_INDEX or FL_DOUBLE.

   If the desired combination cannot be done, FLTK will try turning off
   FL_MULTISAMPLE.  If this also fails the show() will call
   Fl::error() and not show the window.

   You can change the mode while the window is displayed.  This is most
   useful for turning double-buffering on and off.  Under X this will
   cause the old X window to be destroyed and a new one to be created.  If
   this is a top-level window this will unfortunately also cause the
   window to blink, raise to the top, and be de-iconized, and the xid()
   will change, possibly breaking other code.  It is best to make the GL
   window a child of another window if you wish to do this!

   mode() must not be called within draw() since it
   changes the current context.

   The FL_OPENGL3 flag is recommended to use WebGPU version 3 or more.
   This flag is required (not just recommended) if  GL ≥ 3.0 is in use
   and at least one of these conditions applies:
   - the program runs on the macOS platform;
   - the Fl_WebGPU_Window has child widgets.

   See more details in \ref opengl3.

   \version the <tt>FL_OPENGL3</tt> flag appeared in version 1.3.4
   */
  int mode(int a) {return mode(a,0);}
  /** Set the WebGPU capabilities of the window using platform-specific data.
   \param a zero-ending array of platform-specific attributes and attribute values
   \note What attributes are adequate here is subject to change.
   The preferred, stable public API is Fl_WebGPU_Window::mode(int a).
   <p><b>Windows platform</b>: this member function is of no use.
   <p><b>Mac OS X platform</b>: attributes belong to the <tt>CGLPixelFormatAttribute</tt> enumeration
   (defined by including <tt><WebGPU/WebGPU.h></tt>, e.g., <tt>kCGLPFADoubleBuffer</tt>)
   and may be followed by adequate attribute values.
   */
  int mode(const int *a) {return mode(0, a);}
  /** Returns a pointer to the window's WebGPU rendering context.
   \see void context(GLContext c, int destroy_flag)
  */
  void make_current();
  void swap_buffers();
  void swap_interval(int);
  int swap_interval() const;
  void ortho();

  /**
   * Virtual function called to prepare the drawing resources like vertex buffers,
   * pipelines, etc.
   *
   */
  virtual void prepare() {};

  /**
   * Virtual function called to clear up the drawing resources upon a resize or hiding of
   * the window.
   *
   */
  virtual void destroy() {};

  Fl_WebGPU_Window* as_wgpu_window() override { return this; }
  Fl_WebGPU_Window const* as_wgpu_window() const override { return this; }

  float pixels_per_unit();
  /** Gives the window width in WebGPU pixels.
   When an Fl_WebGPU_Window is mapped to a HighDPI display, the value given by Fl_WebGPU_Window::w()
   which is expressed in FLTK units, may differ from the window width in pixels.
   Calls to WebGPU functions expecting pixel values (\em e.g., \c glViewport) should therefore
   use pixel_w() rather than w().
   Method pixel_w() detects when the GUI is rescaled or when the window has been moved
   between low and high resolution displays and automatically adjusts the returned value.
   \version 1.3.4
   */
  int pixel_w() { return int(pixels_per_unit() * w() + 0.5f); }
  /** Gives the window height in WebGPU pixels.
   When an Fl_WebGPU_Window is mapped to a HighDPI display, the value given by Fl_WebGPU_Window::h()
   which is expressed in FLTK units, may differ from the window height in pixels.
   Calls to WebGPU functions expecting pixel values (\em e.g., \c glViewport) should therefore
   use pixel_h() rather than h().
   Method pixel_h() detects when the GUI is rescaled or when the window has been moved
   between low and high resolution displays and automatically adjusts the returned value.
   \version 1.3.4
   */
  int pixel_h() { return int(pixels_per_unit() * h() + 0.5f); }

  ~Fl_WebGPU_Window();
  /**
    Creates a new Fl_WebGPU_Window widget using the given size, and label string.
    The default boxtype is FL_NO_BOX. The default mode is FL_RGB|FL_DOUBLE|FL_DEPTH.
  */
  Fl_WebGPU_Window(int W, int H, const char *l=0) : Fl_Window(W,H,l) {init();}
  /**
    Creates a new Fl_WebGPU_Window widget using the given position,
    size, and label string. The default boxtype is FL_NO_BOX. The
    default mode is FL_RGB|FL_DOUBLE|FL_DEPTH.
  */

  Fl_WebGPU_Window(int X, int Y, int W, int H, const char *l=0)
    : Fl_Window(X,Y,W,H,l) {init();}
};

#endif // Fl_WebGPU_Window_H
