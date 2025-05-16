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

#include <string>
#include <vector>

#include "Fl_Vk_Context.H"
#include "Fl_Window.H"

class Fl_Vk_Choice; // structure to hold result of glXChooseVisual (unused)
class Fl_Vk_Window_Driver;

struct Fl_Vk_SwapchainBuffer {
    VkImage image;
    VkImageView view;
    VkFramebuffer framebuffer;

    Fl_Vk_SwapchainBuffer() :
        image(VK_NULL_HANDLE),
        view(VK_NULL_HANDLE),
        framebuffer(VK_NULL_HANDLE)
        {
        }
    
    void destroy(VkDevice& device)
        {
            if (framebuffer != VK_NULL_HANDLE)
            {
                vkDestroyFramebuffer(device, framebuffer, NULL);
                framebuffer = VK_NULL_HANDLE;
            }

            if (view != VK_NULL_HANDLE)
            {
                vkDestroyImageView(device, view, NULL);
                view = VK_NULL_HANDLE;
            }
            
            // image belongs to the swapchain (must not destroy).
        }
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

    void destroy(VkDevice& device)
        {
            if (view != VK_NULL_HANDLE)
            {
                vkDestroyImageView(device, view, NULL);
                view = VK_NULL_HANDLE;
            }

            if (image != VK_NULL_HANDLE)
            {
                vkDestroyImage(device, image, NULL);
                image = VK_NULL_HANDLE;
            }

            if (mem != VK_NULL_HANDLE)
            {
                vkFreeMemory(device, mem, NULL);
                mem = VK_NULL_HANDLE;
            }
        }
};

/**
  The Fl_Vk_Window widget sets things up so Vulkan works.

  It also keeps an Vulkan "context" for that window.

  Please note that the FLTK drawing and clipping functions
  will not work inside an Fl_Vk_Window. All drawing
  should be done using Vulkan calls exclusively.

*/
class FL_EXPORT Fl_Vk_Window : public Fl_Window {
  friend class Fl_Vk_Window_Driver;
  Fl_Vk_Window_Driver *pVkWindowDriver;

  VkHdrMetadataEXT        m_previous_hdr_metadata;
  int mode_;
  const int *alist;
  Fl_Vk_Choice *g; // needed ?  we may need to use it translate
  char damage1_; // damage() of back buffer (we might use this in the future)
  void init();

  //! Tracking variable to know if begin_render_pass()/end_render_pass() has
  //! been called.
  bool                   m_in_render_pass = false;

  //! Fence used to handle resizes.
  VkFence                m_resizeFence;
  VkExtent2D             m_currentExtent; // Track current swapchain extend

    
  static int can_do(int, const int *);
  int mode(int, const int *);
  void destroy_common_resources();
  
protected:
  void recreate_swapchain();

  //! Called on initialization of the swapchain to set up the swapchain
  //! format and color space.
  virtual void init_colorspace();

  //! Called before begin_render_pass.  Allows setting up a color or depth.
  //! The default function will call begin_render_pass().  Override this if
  //! you don't want that behavior.
  virtual void vk_draw_begin();

  //! Main function to use for drawing with Vulkan.  Note that by default
  //! FLTK calls this after calling vk_draw_begin() and begin_render_pass();
  virtual void draw() FL_OVERRIDE;

  //! Called after draw()
  virtual void vk_draw_end();

  //! Called in case 
  void set_hdr_metadata();

  //! Called to clean up resources when shutting down vulkan.
  void shutdown_vulkan();
  
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

  //! Application and engine name.
  virtual const char* application_name() { return "FLTK"; }
  virtual const char* engine_name() { return "FLTK"; }
  
  //! Main entry point for Vulkan elements creation and destruction.
  virtual void prepare() = 0;
  virtual void destroy() {};  

  //! Required and optional instance extensions used on creation (overload it)
  virtual std::vector<const char*> get_instance_extensions();
  virtual std::vector<const char*> get_optional_extensions();

  //! Required device extensions used on creation (overload it)
  virtual std::vector<const char*> get_device_extensions();

  //! Log level of Vulkan API
  virtual int log_level() const { return 0; }

  //! @{ Accessors to Vulkan elements
  inline VkInstance&             instance() { return m_instance; }
  inline VkPhysicalDevice&            gpu() { return ctx.gpu; }
  inline VkPhysicalDevice& physicalDevice() { return ctx.gpu; }
  inline VkDevice&                 device() { return ctx.device; }
  inline VkPipelineCache&   pipelineCache() { return ctx.pipelineCache; }
  inline VkQueue&                   queue() { return ctx.queue; }
  inline VkCommandPool&       commandPool() { return ctx.commandPool; }
  inline VkFormat&                 format() { return ctx.format; }
  inline VkColorSpaceKHR&      colorSpace() { return ctx.colorSpace; }

  //! Required device extensions
  inline VkRenderPass&         renderPass() { return m_renderPass; }
  inline VkPipeline&             pipeline() { return m_pipeline; }
      
  inline const VkInstance&             instance() const { return m_instance; }
  inline const VkPhysicalDevice&            gpu() const { return ctx.gpu; }
  inline const VkPhysicalDevice& physicalDevice() const { return ctx.gpu; }
  inline const VkDevice&                 device() const { return ctx.device; }
  inline const VkPipelineCache&   pipelineCache() const { return ctx.pipelineCache; }
  inline const VkQueue&                   queue() const { return ctx.queue; }
  inline const VkCommandPool&       commandPool() const { return ctx.commandPool; }
  inline const VkFormat&                 format() const { return ctx.format; }
  inline const VkColorSpaceKHR&      colorSpace() const { return ctx.colorSpace; }
  inline const VkRenderPass&         renderPass() const { return m_renderPass; }
  inline const VkPipeline&             pipeline() const { return m_pipeline; }

  // Returns a vector with currently used insance extensions
  inline const std::vector<const char*>
      getCurrentInstanceExtensions() const { return ctx.instance_extensions; }
  inline const std::vector<const char*>
      getCurrentDeviceExtensions() const { return ctx.device_extensions; }

  inline void setValidationLayers(bool v) { v = m_validate; }
  
  //! @}
  
  // Application

  // if true, turns on validation layers.  Default is to have it on on Debug
  // builds and off on Release or RelWithDebInfo builds.
  bool m_validate;

  // Traces the use of the swapchain.  You probably don't need use it.
  bool m_debugSync;
  
  // These get copied into ctx for every window
  static VkInstance m_instance; 
  static VkDevice   m_device;
  static VkQueue    m_queue;
  
  static PFN_vkSetHdrMetadataEXT vkSetHdrMetadataEXT;  // We handle HDR extension if available.

  // Window's Vulkan context.
  Fl_Vk_Context ctx;

  // Per Window variables

  // For clearing the background (should be set in vk_draw_begin())
  VkClearColorValue  m_clearColor;

  // For clearing the stencil    (should be set in vk_draw_begin())
  float              m_depthStencil;

  //! The surface and its format gotten from the Window driver.
  VkSurfaceKHR       m_surface;
  VkSurfaceFormatKHR m_surfaceFormat;

  //! For drawing (one pipeline and one render pass by default)
  VkRenderPass m_renderPass;
  VkPipeline   m_pipeline;

  //! Allocator callback (unused for now).
  const VkAllocationCallbacks*    m_allocator;

private:
  // Per frame data used internally.
  struct FrameData {
      VkSemaphore imageAcquiredSemaphore;
      VkSemaphore drawCompleteSemaphore;
      VkFence fence;
      VkCommandBuffer commandBuffer;
      bool active;

      FrameData() :
          imageAcquiredSemaphore(VK_NULL_HANDLE),
          drawCompleteSemaphore(VK_NULL_HANDLE),
          fence(VK_NULL_HANDLE),
          commandBuffer(VK_NULL_HANDLE),
          active(false)
          {
          }
  };
  

  // Swapchain
private:

protected:
  uint32_t               m_swapchainImageCount; // Track swapchain image count
  std::vector<Fl_Vk_SwapchainBuffer> m_buffers;
  uint32_t                m_current_buffer;
  
  std::vector<FrameData>  m_frames;
  uint32_t                m_currentFrameIndex;

  //! Flag used to handle resizes (swapchain and other resources recreation)
  bool                   m_swapchain_needs_recreation;
  VkSwapchainKHR         m_swapchain;
  
  // HDR metadata information (passed during swap_buffers)
  bool is_equal_hdr_metadata(const VkHdrMetadataEXT& a,
                             const VkHdrMetadataEXT& b);
  VkHdrMetadataEXT        m_hdr_metadata;
  bool                    m_hdr_metadata_changed;
  
  Fl_Vk_Depth             m_depth;
  uint32_t           m_queueFamilyIndex;


  // For drawing
  

  
  
  VkPhysicalDeviceMemoryProperties m_memory_properties;

  
  void init_vulkan();

  // Auxiliary functions to begin and end the main render pass.
  // Note that FLTK will call begin_render_pass() in vk_draw_begin()
  // and end_render_pass() in vk_draw_end().
  // That is: draw() is called with an active render pass by default.
  void begin_render_pass(VkCommandBuffer&);
  void begin_render_pass();
  void end_render_pass(VkCommandBuffer&);
  void end_render_pass();
  
  void swap_buffers();

public:
  
  void swap_interval(int);
  int swap_interval() const;

  int can_do_overlay() { return false; }

  //! Call this to wait on the Window's queue.
  void wait_queue();

  //! Call this to wait on the Vulkan device.  This will wait for all GPUs
  //! operations to finish.
  void wait_device();
  
  VkExtent2D getCurrentExtent() const { return m_currentExtent; }
  void setDebugSync(bool enable) { m_debugSync = enable; }
  VkCommandBuffer getCurrentCommandBuffer() const {
      return m_frames[m_currentFrameIndex].commandBuffer;
  }
  bool isFrameActive() const {
      return m_frames[m_currentFrameIndex].active;
  }
  uint32_t getCurrentBufferIndex() const {
      return m_current_buffer;
  }
  
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
