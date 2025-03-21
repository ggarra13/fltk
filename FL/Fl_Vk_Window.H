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

#define VERTEX_BUFFER_BIND_ID 0


//! GLFW demo variables
struct Fl_Vk_SwapchainBuffer {
    VkImage image;
    VkCommandBuffer cmd;
    VkImageView view;
};

struct Fl_Vk_Mesh {
    VkBuffer buf;
    VkDeviceMemory mem;

    std::vector<int> v;
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
  happens to the part of the Vulkan subwindow which leaks outside its parent is undefined
  and susceptible to be platform-specific.
*/
class FL_EXPORT Fl_Vk_Window : public Fl_Window {
  friend class Fl_Vk_Window_Driver;
  Fl_Vk_Window_Driver *pVkWindowDriver;

  int mode_;
  const int *alist;
  Fl_Vk_Choice *g; // needed ?  we may need to use it translate
  char damage1_; // damage() of back buffer (we might use this in the future)
  void init();

    
  static int can_do(int, const int *);
  int mode(int, const int *);
protected:

  bool memory_type_from_properties(uint32_t typeBits,
                                   VkFlags requirements_mask,
                                   uint32_t *typeIndex);
  void destroy_texture_image(Fl_Vk_Texture *tex_obj);
  void draw_begin();
  void draw() FL_OVERRIDE;
  void draw_end();

  void flush_init_cmd();

public:
  void show() FL_OVERRIDE;
  /** Same as Fl_Window::show(int a, char **b) */
  void show(int a, char **b) {Fl_Window::show(a,b);}
  void flush() FL_OVERRIDE;
  void hide() FL_OVERRIDE;
  void resize(int,int,int,int) FL_OVERRIDE;
  int handle(int) FL_OVERRIDE;

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
  **/
  int mode(const int *a) {return mode(0, a);}

  VkResult begin_setup();
  virtual void setup() {}; 
  VkResult end_setup();

  virtual void prepare() = 0;
  virtual void destroy_resources();
  

  uint32_t m_enabled_layer_count;
  const char* m_enabled_layers[64];
  // These should be moved somewhere else
  uint32_t m_enabled_extension_count;
  const char* m_extension_names[64];


  
  
  // Application
  bool                      m_validate = false;
  static VkInstance         m_instance; 
  static VkPhysicalDevice   m_gpu;
  static VkPipelineCache    m_pipelineCache; 


  

  // Per Window
  VkSurfaceKHR       m_surface;  // needed
  VkSurfaceFormatKHR m_surfaceFormat; //needed
  VkDevice           m_device;   // needed
  VkFormat           m_format;   // for depth and color formats
  VkRenderPass       m_renderPass; // needed
  VkSwapchainKHR     m_swapchain;
  uint32_t           m_swapchainImageCount;
  VkQueue            m_queue;
  Fl_Vk_SwapchainBuffer*       m_buffers;
  uint32_t           m_queueFamilyIndex;
  Fl_Vk_Depth        m_depth;
  VkFramebuffer*     m_framebuffers;

  // For drawing
  VkPipeline         m_pipeline;  
  VkCommandPool      m_cmd_pool;  // memory for command buffers
  VkDescriptorPool   m_desc_pool; // memory for descriptor sets
  VkCommandBuffer    m_setup_cmd; // Command Buffer for resources
  VkCommandBuffer    m_draw_cmd;  // Command Buffer for drawing commands  
  VkSemaphore        m_imageAcquiredSemaphore;
  VkSemaphore        m_drawCompleteSemaphore;

  // For clearing the background
  VkClearColorValue  m_clearColor;
  float m_depthStencil = 1.0;

  
  VkPipelineLayout      m_pipeline_layout;  // interface between shaders and desc.sets
  VkDescriptorSetLayout m_desc_layout; // describe texture bindings whithin desc. set
  VkDescriptorSet       m_desc_set; // actual data bound to shaders like texture or
                                    // uniform buffers
  

  //! Flag used to handle resizes (swapchain and other resources recreation)
  bool               m_swapchain_needs_recreation;

  // Not sure what this one is for
  bool               m_use_staging_buffer = false;
    

  // Not sure what these are for
  VkPhysicalDeviceProperties m_gpu_props;
  VkPhysicalDeviceFeatures m_gpu_features;
  VkQueueFamilyProperties* m_queue_props;

  
  //!@{ These are from triangle-vulkan GLFW demo
  VkColorSpaceKHR    m_color_space;
  
  VkPhysicalDeviceMemoryProperties m_memory_properties;
  uint32_t                m_current_buffer;
  uint32_t                m_queue_count;

  

    

  //! @}
  
  // (Optional) Allocation, Debugging.
  const VkAllocationCallbacks*    m_allocator;
  void                            (*CheckVkResultFn)(VkResult err);
  VkDeviceSize                    MinAllocationSize;      // Minimum allocatio

  bool               m_use_break = false;
  VkDebugUtilsMessengerEXT  m_debug_messenger;
  
  void make_current();
  void swap_buffers();
  void swap_interval(int);
  int swap_interval() const;

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

  virtual ~Fl_Vk_Window();
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
