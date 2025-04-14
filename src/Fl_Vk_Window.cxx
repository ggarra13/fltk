//
// Vulkan window code for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2022 by Bill Spitzak and others.
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

#include <config.h>

#if HAVE_VK


#include <FL/vk.h>
#include <FL/Fl_Vk_Window.H>
#include "Fl_Vk_Window_Driver.H"
#include "Fl_Window_Driver.H"
#include <FL/Fl_Graphics_Driver.H>
#include <FL/fl_utf8.h>

//! Instance is per application
VkInstance Fl_Vk_Window::m_instance = VK_NULL_HANDLE;


static bool is_equal_hdr_metadata(const VkHdrMetadataEXT& a,
                                  const VkHdrMetadataEXT& b)
{
    return (a.displayPrimaryRed.x == b.displayPrimaryRed.x &&
            a.displayPrimaryRed.y == b.displayPrimaryRed.y &&
            a.displayPrimaryGreen.x == b.displayPrimaryGreen.x &&
            a.displayPrimaryGreen.y == b.displayPrimaryGreen.y &&
            a.displayPrimaryBlue.x == b.displayPrimaryBlue.x &&
            a.displayPrimaryBlue.y == b.displayPrimaryBlue.y &&
            a.whitePoint.x == b.whitePoint.x &&
            a.whitePoint.y == b.whitePoint.y &&
            a.maxLuminance == b.maxLuminance &&
            a.minLuminance == b.minLuminance &&
            a.maxContentLightLevel == b.maxContentLightLevel &&
            a.maxFrameAverageLightLevel == b.maxFrameAverageLightLevel);
}

void Fl_Vk_Window::destroy_resources() {
  if (m_desc_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device(), m_desc_pool, NULL);
    m_desc_pool = VK_NULL_HANDLE;
  }

  if (m_pipeline_layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device(), m_pipeline_layout, NULL);
    m_pipeline_layout = VK_NULL_HANDLE;
  }

  if (m_desc_layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device(), m_desc_layout, NULL);
    m_desc_layout = VK_NULL_HANDLE;
  }

  if (m_pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(device(), m_pipeline, NULL);
    m_pipeline = VK_NULL_HANDLE;
  }

  if (m_renderPass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device(), m_renderPass, NULL);
    m_renderPass = VK_NULL_HANDLE;
  }
}


void Fl_Vk_Window::recreate_swapchain() {
    // Waits for all queue on the device
    vkDeviceWaitIdle(device()); 

    // Destroy window and driver resources
    pVkWindowDriver->destroy_resources(); 

    // Recreate resources
    pVkWindowDriver->prepare();

    set_hdr_metadata();

    m_swapchain_needs_recreation = false; // Reset only if successful
}

void Fl_Vk_Window::vk_draw_begin() {
  VkResult result;

  // Recreate swapchain if needed
  if (m_swapchain_needs_recreation) {
      recreate_swapchain();
  }


  VkSemaphoreCreateInfo semaphoreCreateInfo = {};
  semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphoreCreateInfo.pNext = NULL;
  semaphoreCreateInfo.flags = 0;

  result = vkCreateSemaphore(device(), &semaphoreCreateInfo, NULL, &m_imageAcquiredSemaphore);
  VK_CHECK_RESULT(result);

  result = vkCreateSemaphore(device(), &semaphoreCreateInfo, NULL, &m_drawCompleteSemaphore);
  VK_CHECK_RESULT(result);
  
  // Get the index of the next available swapchain image:
  result = vkAcquireNextImageKHR(device(), m_swapchain, UINT64_MAX, m_imageAcquiredSemaphore,
                                 (VkFence)0, // TODO: Show use of fence
                                 &m_current_buffer);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    // m_swapchain is out of date (e.g. the window was resized) and
    // must be recreated:
    recreate_swapchain();
    vkDestroySemaphore(device(), m_imageAcquiredSemaphore, NULL);
    result = vkCreateSemaphore(device(), &semaphoreCreateInfo, NULL,
                               &m_imageAcquiredSemaphore);
    VK_CHECK_RESULT(result);
    result = vkAcquireNextImageKHR(device(), m_swapchain, UINT64_MAX,
                                   m_imageAcquiredSemaphore,
                                   (VkFence)0, // TODO: Show use of fence
                                   &m_current_buffer);
    if (result != VK_SUCCESS)
    {
        vkDestroySemaphore(device(), m_imageAcquiredSemaphore, NULL);
        vkDestroySemaphore(device(), m_drawCompleteSemaphore, NULL);
        return;
    }
  } else if (result == VK_TIMEOUT) {
    // Timeout occurred, try again next frame
    return;
  } else {
    VK_CHECK_RESULT(result);
  }

  VkCommandBufferBeginInfo cmd_buf_info = {};
  cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  cmd_buf_info.pNext = NULL;
  cmd_buf_info.flags = 0;
  cmd_buf_info.pInheritanceInfo = NULL;

  vkQueueWaitIdle(ctx.queue);
  // Reset the command buffer to ensure it’s reusable
  result = vkResetCommandBuffer(m_draw_cmd, 0);
  VK_CHECK_RESULT(result);

  result = vkBeginCommandBuffer(m_draw_cmd, &cmd_buf_info);
  VK_CHECK_RESULT(result);

  // glClearColor / glClearStencil equivalents
  VkClearValue clear_values[2];
  clear_values[0].color = m_clearColor;
  clear_values[1].depthStencil = {m_depthStencil, 0};

  VkRenderPassBeginInfo rp_begin = {};
  rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp_begin.pNext = NULL;
  rp_begin.renderPass = m_renderPass;
  rp_begin.framebuffer = m_buffers[m_current_buffer].framebuffer;
  rp_begin.renderArea.offset.x = 0;
  rp_begin.renderArea.offset.y = 0;
  rp_begin.renderArea.extent.width = w();
  rp_begin.renderArea.extent.height = h();
  rp_begin.clearValueCount = 2;
  rp_begin.pClearValues = clear_values;

  // Transition swapchain image to COLOR_ATTACHMENT_OPTIMAL for rendering
  VkImageMemoryBarrier image_memory_barrier = {};
  image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  image_memory_barrier.pNext = NULL;
  image_memory_barrier.srcAccessMask = 0; // No prior access (undefined)
  image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_memory_barrier.image = m_buffers[m_current_buffer].image;
  image_memory_barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  
  vkCmdPipelineBarrier(m_draw_cmd,
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,   // No prior work
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // For color writes
                       0, 0, NULL, 0, NULL, 1, &image_memory_barrier);


  bool has_depth = mode() & FL_DEPTH;
  bool has_stencil = mode() & FL_STENCIL;
  if (has_depth || has_stencil)
  {
      VkImageMemoryBarrier barrier = {};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Current layout
      barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // Desired layout
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = m_depth.image;
      barrier.subresourceRange.aspectMask = 0;
      if (has_depth)
          barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
      if (has_stencil)
          barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
      barrier.subresourceRange.baseMipLevel = 0;
      barrier.subresourceRange.levelCount = 1;
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount = 1;
      barrier.srcAccessMask = 0; // No previous access (since it's undefined)
      barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                              VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

      vkCmdPipelineBarrier(
          m_draw_cmd,
          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // Source stage (can be more specific if needed)
          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, // Destination stage
          0,
          0, nullptr, // Memory barriers
          0, nullptr, // Buffer barriers
          1, &barrier // Image barriers
          );
  }

  // Begin rendering
  vkCmdBeginRenderPass(m_draw_cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(m_draw_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

  if (m_desc_set != VK_NULL_HANDLE)
      vkCmdBindDescriptorSets(m_draw_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1,
                              &m_desc_set, 0, NULL);

  // The equivalent of glViewport
  VkViewport viewport = {};
  viewport.width = (float)w();
  viewport.height = (float)h();
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  viewport.x = 0;
  viewport.y = 0;

  vkCmdSetViewport(m_draw_cmd, 0, 1, &viewport);

  // The equvalent of glScissor
  VkRect2D scissor = {};
  scissor.extent.width = w();
  scissor.extent.height = h();
  scissor.offset.x = 0;
  scissor.offset.y = 0;

  vkCmdSetScissor(m_draw_cmd, 0, 1, &scissor);
}


/**
  You must implement this virtual function if you want to draw into the
  overlay.  The overlay is cleared before this is called.  You should
  draw anything that is not clear using Vulkan.  You must use
  vk_color(i) to choose colors (it allocates them from the colormap
  using system-specific calls), and remember that you are in an indexed
  Vulkan mode and drawing anything other than flat-shaded will probably
  not work.

  Both this function and Fl_Vk_Window::draw() should check
  Fl_Vk_Window::valid() and set the same transformation.  If you
  don't your code may not work on other systems.  Depending on the OS,
  and on whether overlays are real or simulated, the Vulkan context may
  be the same or different between the overlay and main window.
*/

/**
 Supports drawing to an Fl_Vk_Window with the FLTK 2D drawing API.
 \see \ref vulkan_with_fltk_widgets
 */

/**
 To be used as a match for a previous call to Fl_Vk_Window::vk_draw_begin().
 \see \ref vulkan_with_fltk_widgets
 */
void Fl_Vk_Window::vk_draw_end() {
  VkResult result;

  // Move to draw_end()
  vkCmdEndRenderPass(m_draw_cmd);

  // Transition swapchain image to PRESENT_SRC_KHR for presentation
  // (this is like gl's swap_buffer()?)
  VkImageMemoryBarrier prePresentBarrier = {};
  prePresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  prePresentBarrier.pNext = NULL;
  prePresentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  prePresentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
  prePresentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  prePresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  prePresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  prePresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  prePresentBarrier.image = m_buffers[m_current_buffer].image;
  prePresentBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  vkCmdPipelineBarrier(m_draw_cmd,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // After color writes
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,          // Before presentation
                       0, 0, NULL, 0, NULL, 1, &prePresentBarrier);

  result = vkEndCommandBuffer(m_draw_cmd);
  VK_CHECK_RESULT(result);
}


////////////////////////////////////////////////////////////////

// The symbol SWAP_TYPE defines what is in the back buffer after doing
// a vkXSwapBuffers().

// The OpenVk documentation says that the contents of the backbuffer
// are "undefined" after vkXSwapBuffers().  However, if we know what
// is in the backbuffers then we can save a good deal of time.  For
// this reason you can define some symbols to describe what is left in
// the back buffer.

// Having not found any way to determine this from vkx (or wvk) I have
// resorted to letting the user specify it with an environment variable,
// VK_SWAP_TYPE, it should be equal to one of these symbols:

// contents of back buffer after vkXSwapBuffers():
#define UNDEFINED 1 // anything
#define SWAP 2      // former front buffer (same as unknown)
#define COPY 3      // unchanged
#define NODAMAGE 4  // unchanged even by X expose() events

static char SWAP_TYPE = 0; // 0 = determine it from environment variable


/**  Returns non-zero if the hardware supports the given or current Vulkan  mode. */
int Fl_Vk_Window::can_do(int a, const int *b) {
  return Fl_Vk_Window_Driver::global()->find(a, b) != 0;
}

void Fl_Vk_Window::show() {
  int need_after = 0;
  if (!shown()) {
    Fl_Window::default_size_range();
    if (!g) {
      g = pVkWindowDriver->find(mode_, alist);
      if (!g && (mode_ & FL_DOUBLE) == FL_SINGLE) {
        g = pVkWindowDriver->find(mode_ | FL_DOUBLE, alist);
        if (g)
          mode_ |= FL_FAKE_SINGLE;
      }

      // \@bug: fails on macOS
      // if (!g) {
      //   Fl::error("Insufficient Vulkan support");
      //   return;
      // }
    }
    pVkWindowDriver->before_show(need_after);
  }
  Fl_Window::show();
  if (need_after)
    pVkWindowDriver->after_show();
}

int Fl_Vk_Window::mode(int m, const int *a) {
  if (m == mode_ && a == alist)
    return 0;
  return pVkWindowDriver->mode_(m, a);
}

/**
  The make_current() method selects the Vulkan context for the
  widget.  It is called automatically prior to the draw() method
  being called and can also be used to implement feedback and/or
  selection within the handle() method.
*/

void Fl_Vk_Window::make_current() {
  pVkWindowDriver->make_current_before();
  if (m_surface == VK_NULL_HANDLE)
  {
      init_vulkan();
  }
  pVkWindowDriver->make_current_after();
  current_ = this;
}

void Fl_Vk_Window::set_hdr_metadata()
{
    if (!vkSetHdrMetadataEXT ||
        m_hdr_metadata.sType != VK_STRUCTURE_TYPE_HDR_METADATA_EXT)
        return;
    vkSetHdrMetadataEXT(device(), 1, &m_swapchain, &m_hdr_metadata);
    m_previous_hdr_metadata = m_hdr_metadata;
}

/**
  The swap_buffers() method swaps the back and front buffers.
  It is called automatically after the draw() method is called.
*/
void Fl_Vk_Window::swap_buffers() {
  VkResult result;
  
  VkPipelineStageFlags pipe_stage_flags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  VkSubmitInfo submit_info = {};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = NULL;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &m_imageAcquiredSemaphore;
  submit_info.pWaitDstStageMask = &pipe_stage_flags;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &m_draw_cmd;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &m_drawCompleteSemaphore;

  vkResetFences(device(), 1, &m_drawFence);
  result = vkQueueSubmit(ctx.queue, 1, &submit_info, m_drawFence);
  VK_CHECK_RESULT(result);
  
  vkWaitForFences(device(), 1, &m_drawFence, VK_TRUE, UINT64_MAX);
  vkResetFences(device(), 1, &m_drawFence);

  if (!is_equal_hdr_metadata(m_previous_hdr_metadata,
                             m_hdr_metadata))
  {
      set_hdr_metadata();
  }

  VkPresentInfoKHR present = {};
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.pNext = NULL;
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &m_drawCompleteSemaphore;
  present.swapchainCount = 1;
  present.pSwapchains = &m_swapchain;
  present.pImageIndices = &m_current_buffer;

  result = vkQueuePresentKHR(ctx.queue, &present);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    m_swapchain_needs_recreation = true;
  } else {
    VK_CHECK_RESULT(result);
  }

  result = vkQueueWaitIdle(ctx.queue);
  VK_CHECK_RESULT(result);
  
  vkDestroySemaphore(device(), m_imageAcquiredSemaphore, NULL);
  vkDestroySemaphore(device(), m_drawCompleteSemaphore, NULL);
  
  pVkWindowDriver->swap_buffers();
}

/**
 Sets the rate at which the VK windows swaps buffers.
 This method can be called after the Vulkan context was created, typically
 within the user overridden `Fl_Vk_Window::draw()` method that will be
 overridden by the user.
 \note This method depends highly on the underlying Vulkan contexts and driver
    implementation. Most driver seem to accept only 0 and 1 to swap buffer
    asynchronously or in sync with the vertical blank.
 \param[in] frames set the number of vertical frame blanks between Vulkan
    buffer swaps
 */
void Fl_Vk_Window::swap_interval(int frames) {
  pVkWindowDriver->swap_interval(frames);
}

/**
 Gets the rate at which the VK windows swaps buffers.
 This method can be called after the Vulkan context was created, typically
 within the user overridden `Fl_Vk_Window::draw()` method that will be
 overridden by the user.
 \note This method depends highly on the underlying Vulkan contexts and driver
    implementation. Some drivers return no information, most drivers don't
    support intervals with multiple frames and return only 0 or 1.
 \note Some drivers have the ability to set the swap interval but no way
    to query it, hence this method may return -1 even though the interval was
    set correctly. Conversely a return value greater zero does not guarantee
    that the driver actually honors the setting.
 \return an integer greater zero if vertical blanking is taken into account
    when swapping Vulkan buffers
 \return 0 if the vertical blanking is ignored
 \return -1 if the information can not be retrieved
 */
int Fl_Vk_Window::swap_interval() const {
  return pVkWindowDriver->swap_interval();
}


void Fl_Vk_Window::flush() {
  if (!shown())
    return;
  if (pVkWindowDriver->flush_begin())
    return;

  make_current();

  // Check if Vulkan resources are ready
  if (m_swapchain == VK_NULL_HANDLE || m_buffers.empty() ||
      m_draw_cmd == VK_NULL_HANDLE) {
    return; // Skip drawing until initialization completes
  }


  if (mode_ & FL_DOUBLE) {


    if (!SWAP_TYPE) {
      SWAP_TYPE = pVkWindowDriver->swap_type();
      const char *c = fl_getenv("VK_SWAP_TYPE");
      if (c) {
        if (!strcmp(c, "COPY"))
          SWAP_TYPE = COPY;
        else if (!strcmp(c, "NODAMAGE"))
          SWAP_TYPE = NODAMAGE;
        else if (!strcmp(c, "SWAP"))
          SWAP_TYPE = SWAP;
        else
          SWAP_TYPE = UNDEFINED;
      }
    }

    if (SWAP_TYPE == NODAMAGE) {

      // don't draw if only overlay damage or expose events:
      if ((damage() & ~(FL_DAMAGE_OVERLAY | FL_DAMAGE_EXPOSE)))
      {
          vk_draw_begin();
          draw();
          vk_draw_end();
      }
      swap_buffers();

    } else if (SWAP_TYPE == COPY) {

      // don't draw if only the overlay is damaged:
      if (damage() != FL_DAMAGE_OVERLAY)
      {
          vk_draw_begin();
          draw();
          vk_draw_end();
      }
      swap_buffers();

    } else if (SWAP_TYPE == SWAP) {
      damage(FL_DAMAGE_ALL);
      vk_draw_begin();
      draw();
      vk_draw_end();
      swap_buffers();
    } else if (SWAP_TYPE == UNDEFINED) { // SWAP_TYPE == UNDEFINED
      damage1_ = damage();
      clear_damage(0xff);
      vk_draw_begin();
      draw();
      vk_draw_end();
      swap_buffers();
    }

  } else { // single-buffered context is simpler:
      vk_draw_begin();
      draw();
      vk_draw_end();
  }
}

void Fl_Vk_Window::resize(int X, int Y, int W, int H) {
  // printf("Fl_Vk_Window::resize(X=%d, Y=%d, W=%d, H=%d)\n", X, Y, W, H);
  // printf("current: x()=%d, y()=%d, w()=%d, h()=%d\n", x(), y(), w(), h());

  int is_a_resize = (W != Fl_Widget::w() || H != Fl_Widget::h() || is_a_rescale());

  Fl_Window::resize(X, Y, W, H);

  if (is_a_resize) {
    m_swapchain_needs_recreation = true;
  }
}

/**
  Hides the window and destroys the Vulkan context.
*/
void Fl_Vk_Window::hide() {
  Fl_Window::hide();
}


/** Draws the Fl_Vk_Window.
  You \e \b must subclass Fl_Vk_Window and provide an implementation for
  draw().

  The draw() method can <I>only</I> use Vulkan calls.  Do not
  attempt to call X, any of the functions in <FL/fl_draw.H>, Vulkan or vk
  directly.  Do not call vk_start() or vk_finish().

  Double-buffering is enabled by default in the window, the back and front
  buffers are swapped after this function is completed.

  \code
  void mywindow::draw() {
  ... draw your geometry here ...
  }
  \endcode

  Actual example code to clear screen to blue:
  \code
  void mywindow::draw() {
  Fl_Vk_Window::draw();
  }
  \endcode

  Regular FLTK widgets can be added as children to the Fl_Vk_Window. To
  correctly overlay the widgets, Fl_Vk_Window::draw() must be called after
  rendering the main scene.
  \code

  void mywindow::vk_draw_begin()
  {
     // Background color
     m_clearColor = { 0.0, 0.0, 0.0, 0.0 };

     // Clear the screen and prepare m_draw_cmd
     Fl_Vk_Window::vk_draw_begin();
  }
  
  void mywindow::draw() {
  // draw 3d graphics scene
  Fl_Vk_Window::draw();
  // -- or --
  Fl_Window::draw();
  // other 2d drawing calls, overlays, etc.
  }
  \endcode

*/
void Fl_Vk_Window::draw() {
  Fl_Window::draw();
}
/**
 Handle some FLTK events as needed.
 */
int Fl_Vk_Window::handle(int event) {
  return Fl_Window::handle(event);
}

/** The number of pixels per FLTK unit of length for the window.
 This method dynamically adjusts its value when the GUI is rescaled or when the window
 is moved to/from displays of distinct resolutions. This method is useful, e.g., to convert,
 in a window's handle() method, the FLTK units returned by Fl::event_x() and
 Fl::event_y() to the pixel units used by the Vulkan source code.
 \version 1.3.4
 */
float Fl_Vk_Window::pixels_per_unit() {
  return pVkWindowDriver->pixels_per_unit();
}


/**
 \cond DriverDev
 \addtogroup DriverDeveloper
 \{
 */

int Fl_Vk_Window_Driver::copy = COPY;
Fl_Window *Fl_Vk_Window_Driver::cached_window = NULL;
float Fl_Vk_Window_Driver::vk_scale =
    1; // scaling factor between FLTK and VK drawing units: VK = FLTK * vk_scale

// creates a unique, dummy Fl_Vk_Window_Driver object used when no Fl_Vk_Window is around
// necessary to support vk_start()/vk_finish()
Fl_Vk_Window_Driver *Fl_Vk_Window_Driver::global() {
  static Fl_Vk_Window_Driver *gwd = newVkWindowDriver(NULL);
  return gwd;
}

void Fl_Vk_Window_Driver::invalidate() {}



char Fl_Vk_Window_Driver::swap_type() {
  return UNDEFINED;
}


Fl_Font_Descriptor **Fl_Vk_Window_Driver::fontnum_to_fontdescriptor(int fnum) {
  extern FL_EXPORT Fl_Fontdesc *fl_fonts;
  return &(fl_fonts[fnum].first);
}

/* Captures a rectangle of a Fl_Vk_Window and returns it as an RGB image.
 This is the platform-independent version. Some platforms may override it.
 */
Fl_RGB_Image *Fl_Vk_Window_Driver::capture_vk_rectangle(int x, int y, int w, int h) {
  return nullptr;
}

/**
  The destructor removes the widget and destroys the Vulkan context
  associated with it.
*/
Fl_Vk_Window::~Fl_Vk_Window() {
  hide();

  if (m_draw_cmd != VK_NULL_HANDLE)
  {
      vkFreeCommandBuffers(device(), commandPool(), 1, &m_draw_cmd);
  }

  if (commandPool() != VK_NULL_HANDLE)
  {
      vkDestroyCommandPool(device(), commandPool(), nullptr);
  }
  
  // Clean up fences and semaphores
  vkDestroyFence(device(), m_drawFence, nullptr);
    
  destroy_resources();
  delete pVkWindowDriver;
}

void Fl_Vk_Window::init_fences()
{
    // Initialize fences and semaphores once
    VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        nullptr, 0 };
    vkCreateFence(device(), &fenceInfo, nullptr, &m_drawFence);
}

void Fl_Vk_Window::init_vk_swapchain()
{
    pVkWindowDriver->init_vk_swapchain();
}

void Fl_Vk_Window::init_vulkan()
{
    VkResult result;
    
    // Initialize vulkan
    if (ctx.instance == VK_NULL_HANDLE)
    {
        pVkWindowDriver->init_vk();
        vkSetHdrMetadataEXT =
            (PFN_vkSetHdrMetadataEXT)
            vkGetDeviceProcAddr(device(), "vkSetHdrMetadataEXT");
    }
  
    pVkWindowDriver->create_surface();
    init_vk_swapchain();  // to allow changing to HDR for example
    pVkWindowDriver->prepare();


    VkCommandPoolCreateInfo cmd_pool_info = {};
    cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_info.pNext = NULL;
    cmd_pool_info.queueFamilyIndex = m_queueFamilyIndex;
    cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    result = vkCreateCommandPool(device(), &cmd_pool_info, NULL, &commandPool());
    VK_CHECK_RESULT(result);

    VkCommandBufferAllocateInfo cmd = {};
    cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd.pNext = NULL;
    cmd.commandPool = commandPool();
    cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd.commandBufferCount = 1;

    result = vkAllocateCommandBuffers(device(), &cmd, &m_draw_cmd);
    VK_CHECK_RESULT(result);
    
    init_fences();
}

std::vector<const char*> Fl_Vk_Window::get_device_extensions()
{
    std::vector<const char*> out;
    return out;
}

std::vector<const char*> Fl_Vk_Window::get_instance_extensions()
{
    std::vector<const char*> out;
    return out;
}

std::vector<const char*> Fl_Vk_Window::get_optional_extensions()
{
    std::vector<const char*> out;
    out.push_back("VK_EXT_swapchain_colorspace");
    return out;
}


void Fl_Vk_Window::create_device() {
    VkResult result;

    float queue_priorities[1] = {0.0};
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.pNext = NULL;
    queueCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = queue_priorities;

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = NULL;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceInfo.enabledLayerCount = ctx.enabled_layers.size();
    deviceInfo.ppEnabledLayerNames = ctx.enabled_layers.data();
    deviceInfo.enabledExtensionCount = ctx.device_extensions.size();
    deviceInfo.ppEnabledExtensionNames = ctx.device_extensions.data();

    result = vkCreateDevice(gpu(), &deviceInfo, NULL, &device());
    VK_CHECK_RESULT(result);
}


void Fl_Vk_Window::init() {
  pVkWindowDriver = Fl_Vk_Window_Driver::newVkWindowDriver(this);
  end(); // we probably don't want any children
  box(FL_NO_BOX);

  mode_ = FL_RGB | FL_DEPTH | FL_DOUBLE;
  alist = 0;
  g = 0;
  damage1_ = 0;

  // Set up defaults
  m_swapchain_needs_recreation = false;
  m_use_staging_buffer = false;
  m_depthStencil = 1.0;
  
  // Reset Vulkan Handles
#ifdef NDEBUG
  m_validate = false;
#else
  m_validate = true;
#endif
  
  m_surface = VK_NULL_HANDLE; // not really needed to keep in class
  
  m_hdr_metadata = {};
  m_previous_hdr_metadata = {};
  vkSetHdrMetadataEXT = nullptr;
  
  m_swapchain = VK_NULL_HANDLE;
  m_renderPass = VK_NULL_HANDLE;
  m_pipeline = VK_NULL_HANDLE;
  m_allocator = nullptr;

  // Draw command and fence (currently unused)
  m_draw_cmd = VK_NULL_HANDLE;
  m_drawFence = VK_NULL_HANDLE;

  // Texture descriptor handles
  m_desc_set  = VK_NULL_HANDLE;
  m_desc_pool = VK_NULL_HANDLE;
  m_desc_layout = VK_NULL_HANDLE;
  
  m_pipeline_layout = VK_NULL_HANDLE;

  // Counters
  m_current_buffer = 0;
}

/**
 \}
 \endcond
 */

#endif // HAVE_VK
