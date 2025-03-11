//
// Vulkan header file for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2023 by Bill Spitzak and others.
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
   Fl_Vk_Window widget . */

#ifndef Fl_Vk_Window_H
#define Fl_Vk_Window_H

#include <Fl/vk.h>
#include "Fl_Window.H"

class Fl_Vk_Choice; // structure to hold result of glXChooseVisual
class Fl_Vk_Window_Driver;

typedef struct {
    VkImage image;
    VkCommandBuffer cmd;
    VkImageView view;
} SwapchainBuffers;

struct Fl_Vk_Frame
{
    VkCommandPool       CommandPool;
    VkCommandBuffer     CommandBuffer;
    VkFence             Fence;
    VkImage             Backbuffer;
    VkImageView         BackbufferView;
    VkFramebuffer       Framebuffer;
};

struct Fl_Vk_FrameSemaphores
{
    VkSemaphore         ImageAcquiredSemaphore;
    VkSemaphore         RenderCompleteSemaphore;
};

/**
  The Fl_Vk_Window widget sets things up so Vulkan works.

  It also keeps an Vulkan "context" for that window, so that changes to the
  lighting and projection may be reused between redraws. Fl_Vk_Window
  also flushes the Vulkan streams and swaps buffers after draw() returns.

  Vulkan hardware typically provides some overlay bit planes, which
  are very useful for drawing UI controls atop your 3D graphics.  If the
  overlay hardware is not provided, FLTK tries to simulate the overlay.
  This works pretty well if your graphics are double buffered, but not
  very well for single-buffered.

  Please note that the FLTK drawing and clipping functions
  will not work inside an Fl_Vk_Window. All drawing
  should be done using Vulkan calls exclusively.

  \see \ref vulkan_highdpi

  \note FLTK 1.4 introduces a driver system for graphic calls. It is now possible
  to add a selection of widgets to an Vulkan window. The widgets will draw on top
  of any Vulkan rendering. The number of supported widgets will increase as the
  driver development improves. Program test/cube.cxx illustrates how to do that.

  \note FLTK expects that when an Fl_Vk_Window is a child of a parent Fl_Window,
  the child window lies entirely inside its parent window. If that's not the case, what
  happens to the part of the GL subwindow which leaks outside its parent is undefined
  and susceptible to be platform-specific.
*/
class FL_EXPORT Fl_Vk_Window : public Fl_Window {
  friend class Fl_Vk_Window_Driver;
  Fl_Vk_Window_Driver *pVkWindowDriver;

  int mode_;
  const int *alist;
  Fl_Vk_Choice *g;
  bool context_;
  char valid_f_;
  char damage1_; // damage() of back buffer
  void init();

    
  static int can_do(int, const int *);
  int mode(int, const int *);
protected:


  
  void draw_begin();
  void draw() FL_OVERRIDE;
  void draw_end();

public:
  void show() FL_OVERRIDE;
  /** Same as Fl_Window::show(int a, char **b) */
  void show(int a, char **b) {Fl_Window::show(a,b);}
  void flush() FL_OVERRIDE;
  void hide() FL_OVERRIDE;
  void resize(int,int,int,int) FL_OVERRIDE;
  int handle(int) FL_OVERRIDE;

  /**
    Is turned off when FLTK creates a new context for this window or
    when the window resizes, and is turned on \e after draw() is called.
    You can use this inside your draw() method to avoid unnecessarily
    initializing Vulkan. Just do this:
    \code
    void mywindow::draw() {
     if (!valid()) {
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
    See char Fl_Vk_Window::valid() const
  */
  void valid(char v) {if (v) valid_f_ |= 1; else valid_f_ &= 0xfe;}
  void invalidate();

  /**
    Will only be set if the
    OpenGL context is created or recreated. It differs from
    Fl_Gl_Window::valid() which is also set whenever the context
    changes size.
  */
  char context_valid() const {return valid_f_ & 2;}
  /**
    See char Fl_Gl_Window::context_valid() const
  */
  void context_valid(char v) {if (v) valid_f_ |= 2; else valid_f_ &= 0xfd;}
  
  /**  Returns non-zero if the hardware supports the given Vulkan mode. */
  static int can_do(int m) {return can_do(m,0);}
  /**  Returns non-zero if the hardware supports the given Vulkan mode.
   \see Fl_Vk_Window::mode(const int *a) */
  static int can_do(const int *m) {return can_do(0, m);}
  /**  Returns non-zero if the hardware supports the current Vulkan mode. */
  int can_do() {return can_do(mode_,alist);}
  /** Returns the current Vulkan capabilities of the window.
   Don't use this if capabilities were set through Fl_Vk_Window::mode(const int *a).
   */
  Fl_Mode mode() const {return (Fl_Mode)mode_;}
  /**
   Set or change the Vulkan capabilities of the window.  The value can be
   any of the following OR'd together:

   - \c FL_RGB - RGB color (not indexed)
   - \c FL_RGB8 - RGB color with at least 8 bits of each color
   - \c FL_INDEX - Indexed mode
   - \c FL_SINGLE - not double buffered
   - \c FL_DOUBLE - double buffered
   - \c FL_ACCUM - accumulation buffer
   - \c FL_ALPHA - alpha channel in color
   - \c FL_DEPTH - depth buffer
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
   
   */
  int mode(int a) {return mode(a,0);}
  /** Set the Vulkan capabilities of the window using platform-specific data.
   \param a zero-ending array of platform-specific attributes and attribute values
   <p><b>Unix/Linux platform</b>: attributes are GLX attributes adequate for the 3rd argument of
   the <tt>glXChooseVisual()</tt> function (e.g., <tt>GLX_DOUBLEBUFFER</tt>, defined by including <GL/glx.h>).
   \note What attributes are adequate here is subject to change.
   The preferred, stable public API is Fl_Vk_Window::mode(int a).
   <p><b>Windows platform</b>: this member function is of no use.
   <p><b>Mac OS X platform</b>: attributes belong to the <tt>CGLPixelFormatAttribute</tt> enumeration
   (defined by including <tt><Vulkan/Vulkan.h></tt>, e.g., <tt>kCGLPFADoubleBuffer</tt>)
   and may be followed by adequate attribute values.
   */
  int mode(const int *a) {return mode(0, a);}

  VkInstance         m_instance;
  VkPhysicalDevice   m_physicalDevice;
  VkDevice           m_device;
  
  uint32_t m_enabled_extension_count;
  uint32_t m_enabled_layer_count;
  const char* m_extension_names[64];
  const char* m_enabled_layers[64];
    
  VkColorSpaceKHR    m_color_space;
  
    
  VkFormat           m_format;
  VkSurfaceKHR       m_surface;
  VkSurfaceFormatKHR m_surfaceFormat;
  VkPresentModeKHR   m_presentMode;
  VkRenderPass       m_renderPass;
  VkPipeline         m_pipeline;  
  VkQueue            m_queue;
  VkQueueFamilyProperties* m_queue_props;
  uint32_t           m_queueFamilyIndex;
  
  // (Optional) Allocation, Debugging
  const VkAllocationCallbacks*    m_allocator;
  void                            (*CheckVkResultFn)(VkResult err);
  VkDeviceSize                    MinAllocationSize;      // Minimum allocatio

  // These are in ImGui but not sure.
  bool               m_useDynamicRendering;
  // Clear screen?
  bool               m_clearEnable;
  VkClearValue       m_clearValue;

  uint32_t           m_curFrame;   // Current frame being rendered to (0 <= FrameIndex < FrameInFlightCount)


  //! Number of simultaneous in-flight frames
  //! (returned by vkGetSwapchainImagesKHR, often derived from min_image_count)
  
  VkSwapchainKHR     m_swapchain;
  uint32_t           m_swapchainImageCount;
  SwapchainBuffers*       m_buffers;
  
  uint32_t           m_semaphoreCount;         // Number of simultaneous in-flight frames + 1, to be able to use it in vkAcquireNextImageKHR
  uint32_t           m_semaphoreIndex;         // Current set of swapchain wait semaphores we're using (needs to be distinct from per frame data)

  Fl_Vk_Frame*            m_frames;
  Fl_Vk_FrameSemaphores*  m_frameSemaphores;

  // These are from triangle-vulkan GLFW demo
  VkPhysicalDeviceMemoryProperties m_memory_properties;
  uint32_t                m_current_buffer;
  uint32_t                m_queue_count;
  uint32_t                m_graphics_queue_node_index;
  
  void make_current();
  void swap_buffers();
  void swap_interval(int);
  int swap_interval() const;
  void ortho();

  int can_do_overlay() { return false; }

  // Note: Doxygen docs in Fl_Widget.H to avoid redundancy.
  Fl_Vk_Window* as_vk_window() FL_OVERRIDE { return this; }
  Fl_Vk_Window const* as_vk_window() const FL_OVERRIDE { return this; }

  float pixels_per_unit();
  /** Gives the window width in Vulkan pixels.
   When an Fl_Vk_Window is mapped to a HighDPI display, the value given by Fl_Vk_Window::w()
   which is expressed in FLTK units, may differ from the window width in pixels.
   Calls to Vulkan functions expecting pixel values (\em e.g., \c glViewport) should therefore
   use pixel_w() rather than w().
   Method pixel_w() detects when the GUI is rescaled or when the window has been moved
   between low and high resolution displays and automatically adjusts the returned value.
   \version 1.3.4
   */
  int pixel_w() { return int(pixels_per_unit() * w() + 0.5f); }
  /** Gives the window height in Vulkan pixels.
   When an Fl_Vk_Window is mapped to a HighDPI display, the value given by Fl_Vk_Window::h()
   which is expressed in FLTK units, may differ from the window height in pixels.
   Calls to Vulkan functions expecting pixel values (\em e.g., \c glViewport) should therefore
   use pixel_h() rather than h().
   Method pixel_h() detects when the GUI is rescaled or when the window has been moved
   between low and high resolution displays and automatically adjusts the returned value.
   \version 1.3.4
   */
  int pixel_h() { return int(pixels_per_unit() * h() + 0.5f); }

  ~Fl_Vk_Window();
  /**
    Creates a new Fl_Vk_Window widget using the given size, and label string.
    The default boxtype is FL_NO_BOX. The default mode is FL_RGB|FL_DOUBLE|FL_DEPTH.
  */
  Fl_Vk_Window(int W, int H, const char *l=0) : Fl_Window(W,H,l) {init();}
  /**
    Creates a new Fl_Vk_Window widget using the given position,
    size, and label string. The default boxtype is FL_NO_BOX. The
    default mode is FL_RGB|FL_DOUBLE|FL_DEPTH.
  */

  Fl_Vk_Window(int X, int Y, int W, int H, const char *l=0)
    : Fl_Window(X,Y,W,H,l) {
      std::cerr << "Create " << X << ", " << Y << " WxH=" << W
                << "x" << H << std::endl;
      init();
  }
};

#endif // Fl_Vk_Window_H
