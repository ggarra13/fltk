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

#include "FL/vk.h"
#include "Fl_Window.H"

class Fl_Vk_Choice; // structure to hold result of glXChooseVisual (unused)
class Fl_Vk_Window_Driver;

#define DEMO_TEXTURE_COUNT 1
#define VERTEX_BUFFER_BIND_ID 0

static const uint32_t fragShaderCode[] = {
    0x07230203,0x00010000,0x00080007,0x00000014,0x00000000,0x00020011,0x00000001,0x0006000b,
    0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
    0x0007000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x00000011,0x00030010,
    0x00000004,0x00000007,0x00030003,0x00000002,0x00000190,0x00090004,0x415f4c47,0x735f4252,
    0x72617065,0x5f657461,0x64616873,0x6f5f7265,0x63656a62,0x00007374,0x00090004,0x415f4c47,
    0x735f4252,0x69646168,0x6c5f676e,0x75676e61,0x5f656761,0x70303234,0x006b6361,0x00040005,
    0x00000004,0x6e69616d,0x00000000,0x00050005,0x00000009,0x61724675,0x6c6f4367,0x0000726f,
    0x00030005,0x0000000d,0x00786574,0x00050005,0x00000011,0x63786574,0x64726f6f,0x00000000,
    0x00040047,0x00000009,0x0000001e,0x00000000,0x00040047,0x0000000d,0x00000022,0x00000000,
    0x00040047,0x0000000d,0x00000021,0x00000000,0x00040047,0x00000011,0x0000001e,0x00000000,
    0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,
    0x00040017,0x00000007,0x00000006,0x00000004,0x00040020,0x00000008,0x00000003,0x00000007,
    0x0004003b,0x00000008,0x00000009,0x00000003,0x00090019,0x0000000a,0x00000006,0x00000001,
    0x00000000,0x00000000,0x00000000,0x00000001,0x00000000,0x0003001b,0x0000000b,0x0000000a,
    0x00040020,0x0000000c,0x00000000,0x0000000b,0x0004003b,0x0000000c,0x0000000d,0x00000000,
    0x00040017,0x0000000f,0x00000006,0x00000002,0x00040020,0x00000010,0x00000001,0x0000000f,
    0x0004003b,0x00000010,0x00000011,0x00000001,0x00050036,0x00000002,0x00000004,0x00000000,
    0x00000003,0x000200f8,0x00000005,0x0004003d,0x0000000b,0x0000000e,0x0000000d,0x0004003d,
    0x0000000f,0x00000012,0x00000011,0x00050057,0x00000007,0x00000013,0x0000000e,0x00000012,
    0x0003003e,0x00000009,0x00000013,0x000100fd,0x00010038
};

static const uint32_t vertShaderCode[] = {
    0x07230203,0x00010000,0x00080007,0x00000018,0x00000000,0x00020011,0x00000001,0x0006000b,
    0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
    0x0009000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000b,0x00000010,
    0x00000014,0x00030003,0x00000002,0x00000190,0x00090004,0x415f4c47,0x735f4252,0x72617065,
    0x5f657461,0x64616873,0x6f5f7265,0x63656a62,0x00007374,0x00090004,0x415f4c47,0x735f4252,
    0x69646168,0x6c5f676e,0x75676e61,0x5f656761,0x70303234,0x006b6361,0x00040005,0x00000004,
    0x6e69616d,0x00000000,0x00050005,0x00000009,0x63786574,0x64726f6f,0x00000000,0x00040005,
    0x0000000b,0x72747461,0x00000000,0x00060005,0x0000000e,0x505f6c67,0x65567265,0x78657472,
    0x00000000,0x00060006,0x0000000e,0x00000000,0x505f6c67,0x7469736f,0x006e6f69,0x00030005,
    0x00000010,0x00000000,0x00030005,0x00000014,0x00736f70,0x00040047,0x00000009,0x0000001e,
    0x00000000,0x00040047,0x0000000b,0x0000001e,0x00000001,0x00050048,0x0000000e,0x00000000,
    0x0000000b,0x00000000,0x00030047,0x0000000e,0x00000002,0x00040047,0x00000014,0x0000001e,
    0x00000000,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,
    0x00000020,0x00040017,0x00000007,0x00000006,0x00000002,0x00040020,0x00000008,0x00000003,
    0x00000007,0x0004003b,0x00000008,0x00000009,0x00000003,0x00040020,0x0000000a,0x00000001,
    0x00000007,0x0004003b,0x0000000a,0x0000000b,0x00000001,0x00040017,0x0000000d,0x00000006,
    0x00000004,0x0003001e,0x0000000e,0x0000000d,0x00040020,0x0000000f,0x00000003,0x0000000e,
    0x0004003b,0x0000000f,0x00000010,0x00000003,0x00040015,0x00000011,0x00000020,0x00000001,
    0x0004002b,0x00000011,0x00000012,0x00000000,0x00040020,0x00000013,0x00000001,0x0000000d,
    0x0004003b,0x00000013,0x00000014,0x00000001,0x00040020,0x00000016,0x00000003,0x0000000d,
    0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003d,
    0x00000007,0x0000000c,0x0000000b,0x0003003e,0x00000009,0x0000000c,0x0004003d,0x0000000d,
    0x00000015,0x00000014,0x00050041,0x00000016,0x00000017,0x00000010,0x00000012,0x0003003e,
    0x00000017,0x00000015,0x000100fd,0x00010038
};

//! GLFW demo variables
struct Fl_Vk_SwapchainBuffer {
    VkImage image;
    VkCommandBuffer cmd;
    VkImageView view;
};

struct Fl_Vk_Mesh {
    VkBuffer buf;
    VkDeviceMemory mem;

    VkPipelineVertexInputStateCreateInfo vi;
    VkVertexInputBindingDescription vi_bindings[1];
    VkVertexInputAttributeDescription vi_attrs[2];
};

  
struct Fl_Vk_Texture {
    VkSampler sampler;
    
    VkImage image;
    VkImageLayout imageLayout;

    VkDeviceMemory mem;
    VkImageView view;
    int32_t tex_width, tex_height;
};

  
struct Fl_Vk_Depth {
    VkFormat format;
    
    VkImage image;
    VkDeviceMemory mem;
    VkImageView view;

    Fl_Vk_Depth() :
        image(VK_NULL_HANDLE),
        mem(VK_NULL_HANDLE),
        view(VK_NULL_HANDLE)
        {
        };
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

  bool               m_validate = true;  // true: Windows only so far
  bool               m_use_staging_buffer = false;
  VkInstance         m_instance;
  VkPhysicalDevice   m_gpu;
  VkDevice           m_device;
  int                m_width, m_height;
  
    
  VkFormat           m_format;
  VkSurfaceKHR       m_surface;
  VkSurfaceFormatKHR m_surfaceFormat;
  bool               swapchain_needs_recreation;
  VkSwapchainKHR     m_swapchain;
  uint32_t           m_swapchainImageCount;
  VkRenderPass       m_renderPass;
  VkPipeline         m_pipeline;  
  VkQueue            m_queue;

  VkPhysicalDeviceProperties m_gpu_props;
  VkPhysicalDeviceFeatures m_gpu_features;
  VkQueueFamilyProperties* m_queue_props;
  uint32_t           m_queueFamilyIndex;

  
  //!@{ These are from triangle-vulkan GLFW demo

  
  uint32_t m_enabled_extension_count;
  uint32_t m_enabled_layer_count;
  const char* m_extension_names[64];
  const char* m_enabled_layers[64];
    
  VkColorSpaceKHR    m_color_space;
  
  VkPhysicalDeviceMemoryProperties m_memory_properties;
  uint32_t                m_current_buffer;
  uint32_t                m_queue_count;
  uint32_t                m_graphics_queue_node_index;

  Fl_Vk_Depth m_depth;
  Fl_Vk_Mesh m_vertices;

  Fl_Vk_Texture  m_textures[DEMO_TEXTURE_COUNT];
  VkFramebuffer* m_framebuffers;

  VkCommandPool m_cmd_pool;

  Fl_Vk_SwapchainBuffer*       m_buffers;
    
  // Clear screen
  VkShaderModule m_vert_shader_module;
  VkShaderModule m_frag_shader_module;

  VkDescriptorPool m_desc_pool;
  VkDescriptorSet m_desc_set;
  float m_depthStencil = 1.0;
    
  VkCommandBuffer m_setup_cmd; // Command Buffer for initialization commands
  VkCommandBuffer m_draw_cmd;  // Command Buffer for drawing commands
  VkPipelineLayout m_pipeline_layout;
  VkDescriptorSetLayout m_desc_layout;
  VkPipelineCache m_pipelineCache;

  //! @}
  
  // (Optional) Allocation, Debugging.
  const VkAllocationCallbacks*    m_allocator;
  void                            (*CheckVkResultFn)(VkResult err);
  VkDeviceSize                    MinAllocationSize;      // Minimum allocatio

  bool               m_use_break = false;
  VkDebugReportCallbackEXT m_msg_callback;
  
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
    : Fl_Window(X,Y,W,H,l) { init(); }
};

#endif // Fl_Vk_Window_H
