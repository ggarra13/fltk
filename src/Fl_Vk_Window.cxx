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
#include <FL/vk_enum_string_helper.h>
#include <FL/Fl_Vk_Window.H>
#include "Fl_Vk_Window_Driver.H"
#include "Fl_Window_Driver.H"
#include "Fl_Scalable_Graphics_Driver.H" // Fl_Fontdesc
#include <FL/Fl_Graphics_Driver.H>
#include <FL/fl_utf8.h>
#include <FL/platform.H>

#include <iostream>

#define FLTK_ADD_DEVICE_EXTENSION(name) \
    if (!ctx.name) \
    { \
        ctx.name = reinterpret_cast<PFN_##name>( \
            vkGetDeviceProcAddr(device(), #name)); \
        if (!ctx.name) \
        { \
            throw std::runtime_error("Failed to get " #name " function pointer!"); \
        } \
    }


#ifdef USE_INFINITE_TIMEOUT
static const uint64_t kFenceTimeout = UINT64_MAX;
static const uint64_t kAcquireTimeout = UINT64_MAX;
#else
static const uint64_t kFenceTimeout = 1'000'000'000;
static const uint64_t kAcquireTimeout = 1'000'000'000;
#endif

static bool all_windows_invisible()
{
    for (Fl_Window* win = Fl::first_window(); win != nullptr; win = Fl::next_window(win))
    {
        if (win->visible())
        {
            return false;
        }
    }
    return true;
}


//! Per application variables (statics).
VkInstance              Fl_Vk_Window::m_instance = VK_NULL_HANDLE;
VkDevice                Fl_Vk_Window::m_device = VK_NULL_HANDLE;
VmaAllocator            Fl_Vk_Window::m_allocator = VK_NULL_HANDLE;
Fl_Vk_Queue*            Fl_Vk_Window::m_queue = nullptr;
PFN_vkSetHdrMetadataEXT Fl_Vk_Window::vkSetHdrMetadataEXT = nullptr;


bool Fl_Vk_Window::is_equal_hdr_metadata(const VkHdrMetadataEXT& a,
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

void Fl_Vk_Window::destroy_common_resources() {
    // Destroy derived Window's resources
    destroy();
    
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
    VkResult result;

    // Wait for all operations to complete on the device, not queue.
    wait_device();

    // Validate surface
    VkSurfaceCapabilitiesKHR surfCapabilities;
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu(), m_surface, &surfCapabilities);
    if (result == VK_ERROR_SURFACE_LOST_KHR || result != VK_SUCCESS) {
        pVkWindowDriver->destroy_surface();
        pVkWindowDriver->create_surface();
        if (m_surface == VK_NULL_HANDLE) {
            fprintf(stderr, "Failed to recreate surface\n");
            m_swapchain_needs_recreation = true;
            return;
        }
    }
    
    // Free existing command buffers
    for (auto& frame : m_frames)
    {
        if (frame.commandBuffer != VK_NULL_HANDLE)
        {
            // Reset command buffer to release any resource references
            result = vkResetCommandBuffer(frame.commandBuffer, 0);
            if (result != VK_SUCCESS) {
                fprintf(stderr, "vkResetCommandBuffer failed for frame %p: %s\n",
                        &frame, string_VkResult(result));
            }
            vkFreeCommandBuffers(device(), commandPool(), 1, &frame.commandBuffer);
            frame.commandBuffer = VK_NULL_HANDLE;
        }
        if (frame.fence != VK_NULL_HANDLE && frame.submitted)
        {
            result = vkWaitForFences(device(), 1, &frame.fence, VK_TRUE,
                                     kFenceTimeout);
            if (result != VK_SUCCESS) {
                fprintf(stderr, "vkWaitForFences failed during cleanup: %s\n",
                        string_VkResult(result));
                return;
            }
        }
        if (frame.imageAcquiredSemaphore != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(device(), frame.imageAcquiredSemaphore, nullptr);
            frame.imageAcquiredSemaphore = VK_NULL_HANDLE;
        }
        if (frame.fence != VK_NULL_HANDLE)
        {
            vkDestroyFence(device(), frame.fence, nullptr);
            frame.fence = VK_NULL_HANDLE;
        }
        frame.active = false;
    }

    // Destroy old command pool
    if (commandPool() != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(device(), commandPool(), nullptr);
        commandPool() = VK_NULL_HANDLE;
    }

    // Destroy old swapchain resources
    pVkWindowDriver->destroy_resources();

    // Create new command pool
    VkCommandPoolCreateInfo cmd_pool_info = {};
    cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_info.queueFamilyIndex = ctx.queueFamilyIndex;
    cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    result = vkCreateCommandPool(device(), &cmd_pool_info, nullptr, &commandPool());
    if (result != VK_SUCCESS)
    {
        fprintf(stderr, "vkCreateCommandPool failed: %s\n", string_VkResult(result));
        wait_device();
        pVkWindowDriver->destroy_resources();
        m_swapchain = VK_NULL_HANDLE;
        m_swapchain_needs_recreation = true;
        return;
    }
    
    // Recreate swapchain
    pVkWindowDriver->prepare();
    if (m_swapchain == VK_NULL_HANDLE)
    {
        fprintf(stderr, "recreate_swapchain: prepare() failed\n");
        m_swapchain_needs_recreation = true;
        return;
    }

    // Get swapchain image count
    result = vkGetSwapchainImagesKHR(device(), m_swapchain, &m_swapchainImageCount, nullptr);
    if (result != VK_SUCCESS || m_swapchainImageCount == 0)
    {
        fprintf(stderr, "vkGetSwapchainImagesKHR failed: %s\n", string_VkResult(result));
        wait_device();
        pVkWindowDriver->destroy_resources();
        m_swapchain = VK_NULL_HANDLE;
        m_swapchain_needs_recreation = true;
        return;
    }

    // Resize frame data
    m_frames.resize(m_swapchainImageCount);
    VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo = {
        VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        nullptr,
        VK_FENCE_CREATE_SIGNALED_BIT };  // MUST be created signaled
    VkCommandBufferAllocateInfo cmdInfo = {};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdInfo.commandPool = commandPool();
    cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdInfo.commandBufferCount = 1;

    for (auto& frame : m_frames) {
        if (frame.imageAcquiredSemaphore == VK_NULL_HANDLE) {
            result = vkCreateSemaphore(device(), &semaphoreInfo, nullptr, &frame.imageAcquiredSemaphore);
            if (result != VK_SUCCESS) {
                fprintf(stderr, "vkCreateSemaphore (imageAcquired) failed: %s\n", string_VkResult(result));
                shutdown_vulkan();
                return;
            }
        }
        if (frame.fence == VK_NULL_HANDLE) {
            result = vkCreateFence(device(), &fenceInfo, nullptr, &frame.fence);
            if (result != VK_SUCCESS) {
                fprintf(stderr, "vkCreateFence failed: %s\n", string_VkResult(result));
                shutdown_vulkan();
                return;
            }
        }
        if (frame.commandBuffer == VK_NULL_HANDLE) {
            result = vkAllocateCommandBuffers(device(), &cmdInfo, &frame.commandBuffer);
            if (result != VK_SUCCESS) {
                fprintf(stderr, "vkAllocateCommandBuffers failed: %s\n", string_VkResult(result));
                shutdown_vulkan();
                return;
            }
        }
        frame.active = false;
    }
    
    // \@bug: this makes the code crash on draw() to invalid command buffer
    // m_currentFrameIndex = 0;  
    m_swapchain_needs_recreation = false;
}

void Fl_Vk_Window::begin_render_pass(VkCommandBuffer cmd)
{   
    VkClearValue clear_values[2];
    clear_values[0].color = m_clearColor;
    clear_values[1].depthStencil = {m_depthStencil, 0};

    VkRenderPassBeginInfo rp_begin = {};
    rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.renderPass = m_renderPass;
    rp_begin.framebuffer = m_buffers[m_current_buffer].framebuffer;
    rp_begin.renderArea.offset.x = 0;
    rp_begin.renderArea.offset.y = 0;

    int W, H;
    pVkWindowDriver->get_size(W, H);
    
    rp_begin.renderArea.extent.width = W;
    rp_begin.renderArea.extent.height = H;
    rp_begin.clearValueCount = (mode() & FL_DEPTH || mode() & FL_STENCIL) ? 2 : 1;
    rp_begin.pClearValues = clear_values;

    vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    m_in_render_pass = true;
}

void Fl_Vk_Window::begin_render_pass()
{    
    VkCommandBuffer cmd = getCurrentCommandBuffer();
    begin_render_pass(cmd);
}

void Fl_Vk_Window::end_render_pass(VkCommandBuffer cmd)
{
    if (m_in_render_pass)
    {
        vkCmdEndRenderPass(cmd);
        m_in_render_pass = false;
    }
}

void Fl_Vk_Window::end_render_pass()
{
    FrameData& frame = m_frames[m_currentFrameIndex];
    if (m_swapchain == VK_NULL_HANDLE || frame.commandBuffer == VK_NULL_HANDLE
        || !frame.active)
    {
        fprintf(stderr, "Skipping vk_draw_end: Invalid state\n");
        frame.active = false;
        return;
    }
    end_render_pass(frame.commandBuffer);
}

bool Fl_Vk_Window::vk_draw_begin() {
    VkResult result;

    // Check if Vulkan is initialized
    if (m_swapchain == VK_NULL_HANDLE)
    {
        if (m_debugSync)
        {
            fprintf(stderr, "Skipping vk_draw_begin: No swapchain\n");
        }
        return false;
    }

    // Recreate swapchain if needed
    if (m_swapchain_needs_recreation)
    {
        recreate_swapchain();
        if (m_swapchain == VK_NULL_HANDLE)
        {
            fprintf(stderr, "Skipping vk_draw_begin: Swapchain recreation failed\n");
            return false;
        }
    
        m_pixels_per_unit = pixels_per_unit();
    }

    // Get current frame data
    FrameData& frame = m_frames[m_currentFrameIndex];

    // Wait for this frame’s previous use
    if (frame.active && frame.fence != VK_NULL_HANDLE && frame.submitted)
    {
        if (m_debugSync) {
            fprintf(stderr, "Waiting for frame %u fence\n", m_currentFrameIndex);
        }
        result = vkWaitForFences(device(), 1, &frame.fence, VK_TRUE,
                                 kFenceTimeout);
        
        if (result == VK_TIMEOUT) {
            fprintf(stderr, "vkWaitForFences timed out, attempting recovery\n");
            m_swapchain_needs_recreation = true;
            return false;
        }
        if (result != VK_SUCCESS) {
            fprintf(stderr, "Fl_Vk_Window - vkWaitForFences failed: %s\n", string_VkResult(result));
            frame.active = false;
            return false;
        }
        frame.active = false;
    }


    // Acquire next swapchain image
    if (m_debugSync)
    {
        fprintf(stderr, "Acquiring image for frame %u\n", m_currentFrameIndex);
    }
    result = vkAcquireNextImageKHR(device(), m_swapchain, kAcquireTimeout,
                                   frame.imageAcquiredSemaphore, VK_NULL_HANDLE,
                                   &m_current_buffer);
    if (result == VK_TIMEOUT)
    {
        if (m_debugSync)
        {
            fprintf(stderr, "vkAcquireNextImageKHR timed out\n");
        }
        return false;
    }
    else if (result == VK_ERROR_SURFACE_LOST_KHR)
    {
        fprintf(stderr, "Surface lost, triggering recreation\n");
        m_swapchain_needs_recreation = true;
        return false; // Early return to trigger recreate_swapchain
    }
    else if (result == VK_ERROR_OUT_OF_DATE_KHR ||
             result == VK_SUBOPTIMAL_KHR ||
             result == VK_NOT_READY)
    {
        m_swapchain_needs_recreation = true;
        recreate_swapchain();
        if (m_swapchain == VK_NULL_HANDLE)
        {
            if (m_debugSync) {
                fprintf(stderr, "Skipping vk_draw_begin: Swapchain recreation failed\n");
            }
            return false;
        }
        result = vkAcquireNextImageKHR(device(), m_swapchain, kAcquireTimeout,
                                       frame.imageAcquiredSemaphore, VK_NULL_HANDLE,
                                       &m_current_buffer);
        if (result != VK_SUCCESS)
        {
            fprintf(stderr, "vkAcquireNextImageKHR retry failed: %s\n", string_VkResult(result));
            return false;
        }
    }
    else if (result != VK_SUCCESS)
    {
        fprintf(stderr, "vkAcquireNextImageKHR failed: %s\n", string_VkResult(result));
        return false;
    }

    if (m_debugSync) {
        fprintf(stderr, "Acquired image index %u for frame %u\n", m_current_buffer, m_currentFrameIndex);
    }

    // Reset and begin command buffer
    if (!frame.commandBuffer)
    {
        fprintf(stderr, "No command buffer for frame %u\n", m_currentFrameIndex);
        return false;
    }

    result = vkResetCommandBuffer(frame.commandBuffer, 0);
    if (result != VK_SUCCESS)
    {
        fprintf(stderr, "vkResetCommandBuffer failed: %s\n", string_VkResult(result));
        return false;
    }

    VkCommandBufferBeginInfo cmd_buf_info = {};
    cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    result = vkBeginCommandBuffer(frame.commandBuffer, &cmd_buf_info);
    if (result != VK_SUCCESS || frame.commandBuffer == VK_NULL_HANDLE)
    {
        fprintf(stderr, "vkBeginCommandBuffer failed: %s\n", string_VkResult(result));
        return false;
    }

    // Handle depth/stencil
    bool has_depth = mode() & FL_DEPTH;
    bool has_stencil = mode() & FL_STENCIL;
    if (has_depth || has_stencil) {
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
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
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(frame.commandBuffer,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
    
    begin_render_pass(frame.commandBuffer);

    frame.submitted = false;
    frame.active = true;
    return true;
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
void Fl_Vk_Window::vk_draw_end()
{
    end_render_pass();

    FrameData& frame = m_frames[m_currentFrameIndex];
    if (m_swapchain == VK_NULL_HANDLE || frame.commandBuffer == VK_NULL_HANDLE
        || !frame.active) {
        if (m_debugSync) {
            fprintf(stderr, "Skipping vk_draw_end: Invalid state\n");
        }
        frame.active = false;
        return;
    }

    VkResult result = vkEndCommandBuffer(frame.commandBuffer);
    if (result != VK_SUCCESS)
    {
        fprintf(stderr, "vkEndCommandBuffer failed: %s\n", string_VkResult(result));
        frame.active = false;
        return;
    }
}

/**  Returns non-zero if the hardware supports the given or current Vulkan  mode. */
int Fl_Vk_Window::can_do(int a, const int *b) {
  return Fl_Vk_Window_Driver::global()->find(a, b) != 0;
}

void Fl_Vk_Window::show() {
  int need_after = 0;
  if (!shown()) {
    Fl_Window::default_size_range();
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


void Fl_Vk_Window::set_hdr_metadata()
{
    if (!vkSetHdrMetadataEXT || m_hdr_metadata.sType != VK_STRUCTURE_TYPE_HDR_METADATA_EXT) {
        return;
    }
    m_hdr_metadata_changed = true; // Mark as changed
}


void Fl_Vk_Window::swap_buffers() {
    VkResult result;
    
    // Check state
    FrameData& frame = m_frames[m_currentFrameIndex];
    if (m_swapchain == VK_NULL_HANDLE ||
        frame.commandBuffer == VK_NULL_HANDLE || !frame.active) {
        if (m_debugSync) {
            fprintf(stderr, "Skipping swap_buffers: Invalid state\n");
        }
        frame.active = false;
        return;
    }

    Fl_Vk_SwapchainBuffer& buffer = m_buffers[m_current_buffer];

    // Submit command buffer
    VkPipelineStageFlags pipe_stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &frame.imageAcquiredSemaphore;
    submit_info.pWaitDstStageMask = &pipe_stage_flags;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &frame.commandBuffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &buffer.semaphore;

    if (m_debugSync) {
        fprintf(stderr, "Submitting frame %u for image index %u\n",
                m_currentFrameIndex, m_current_buffer);
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex());

        // Reset fence
        if (frame.fence != VK_NULL_HANDLE)
        {
            result = vkResetFences(device(), 1, &frame.fence);
            if (result != VK_SUCCESS)
            {
                fprintf(stderr, "vkResetFences failed: %s\n", string_VkResult(result));
                frame.active = false;
                return;
            }
        }
    
        result = vkQueueSubmit(queue(), 1, &submit_info, frame.fence);
        if (result != VK_SUCCESS) {
            fprintf(stderr, "vkQueueSubmit failed: %s\n",
                    string_VkResult(result));
            frame.active = false;
            return;
        }

        frame.submitted = true;


        pVkWindowDriver->swap_buffers();

        // Update HDR metadata if changed
        if (m_hdr_metadata_changed && vkSetHdrMetadataEXT &&
            m_hdr_metadata.sType == VK_STRUCTURE_TYPE_HDR_METADATA_EXT)
        {
            vkSetHdrMetadataEXT(device(), 1, &m_swapchain, &m_hdr_metadata);
            m_previous_hdr_metadata = m_hdr_metadata;
            m_hdr_metadata_changed = false;
        }
        
        // Present swapchain image
        VkPresentInfoKHR present_info = {};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &buffer.semaphore;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &m_swapchain;
        present_info.pImageIndices = &m_current_buffer;

        if (m_debugSync) {
            fprintf(stderr, "Presenting image index %u for frame %u\n",
                    m_current_buffer, m_currentFrameIndex);
        }
        
        result = vkQueuePresentKHR(queue(), &present_info);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
            result == VK_NOT_READY) {
            m_swapchain_needs_recreation = true;
            return;
        }
        else if (result != VK_SUCCESS)
        {
            fprintf(stderr, "vkQueuePresentKHR failed: %s\n",
                    string_VkResult(result));
            frame.active = false;
            return;
        }
    }

    // Advance to next frame
    m_currentFrameIndex = (m_currentFrameIndex + 1) % m_frames.size();
    m_last_presented_buffer = m_current_buffer;
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
  if (!shown() || pixel_w() <= 0 || pixel_h() <= 0)
      return;

  // Initialize Vulkan
  if (m_swapchain == VK_NULL_HANDLE) {
      init_vulkan();
      if (m_swapchain == VK_NULL_HANDLE) {
          fprintf(stderr, "Vulkan initialization failed\n");
          return;
      }
  }
  
  if (pVkWindowDriver->flush_begin())
      return;
      
  if (!vk_draw_begin())
  {
      fprintf(stderr, "Skipping draw due to Vulkan error\n");
      return;
  }
  draw();  // User defined virtual draw function
  vk_draw_end();
  
  // Submit and present
  swap_buffers();
}

void Fl_Vk_Window::resize(int X, int Y, int W, int H) {
  // printf("Fl_Vk_Window::resize(X=%d, Y=%d, W=%d, H=%d)\n", X, Y, W, H);
  // printf("orig: x()=%d, y()=%d, w()=%d, h()=%d pixel_w()=%d pixel_h()=%d\n",
  //        x(), y(), w(), h(), pixel_w(), pixel_h());
  int is_a_resize = (W != Fl_Widget::w() || H != Fl_Widget::h() ||
                     is_a_rescale() ||
                     m_pixels_per_unit <= 0.F ||
                     m_pixels_per_unit != pixels_per_unit());

  Fl_Window::resize(X, Y, W, H);
  pVkWindowDriver->resize(is_a_resize, W, H);

  if (is_a_resize) {
      m_swapchain_needs_recreation = true;
  }
}

/**
  Hides the window and destroys the Vulkan context.
*/
void Fl_Vk_Window::hide() {
  shutdown_vulkan();
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

Fl_Window *Fl_Vk_Window_Driver::cached_window = NULL;
// scaling factor between FLTK and VK drawing units: VK = FLTK * vk_scale
float Fl_Vk_Window_Driver::vk_scale = 1;

// creates a unique, dummy Fl_Vk_Window_Driver object used when no Fl_Vk_Window is around
// necessary to support vk_start()/vk_finish()
Fl_Vk_Window_Driver *Fl_Vk_Window_Driver::global() {
  static Fl_Vk_Window_Driver *gwd = newVkWindowDriver(NULL);
  return gwd;
}

void Fl_Vk_Window_Driver::invalidate() {}




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

void Fl_Vk_Window::shutdown_vulkan() {
    if (device() == VK_NULL_HANDLE) return;

    VkResult result;

    wait_device();

    // Free command buffers
    for (auto& frame : m_frames) {
        if (frame.commandBuffer != VK_NULL_HANDLE) {
            // Reset command buffer to release any resource references
            result = vkResetCommandBuffer(frame.commandBuffer, 0);
            if (result != VK_SUCCESS) {
                fprintf(stderr, "vkResetCommandBuffer failed for frame %p: %s\n",
                        &frame, string_VkResult(result));
            }
            vkFreeCommandBuffers(device(), commandPool(), 1, &frame.commandBuffer);
            frame.commandBuffer = VK_NULL_HANDLE;
        }
        if (frame.imageAcquiredSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device(), frame.imageAcquiredSemaphore, nullptr);
            frame.imageAcquiredSemaphore = VK_NULL_HANDLE;
        }
        if (frame.fence != VK_NULL_HANDLE) {
            // Wait for fence to ensure no pending operations
            result = vkWaitForFences(device(), 1, &frame.fence, VK_TRUE, UINT64_MAX);
            if (result != VK_SUCCESS) {
                fprintf(stderr, "vkWaitForFences failed for frame %p: %s\n",
                        &frame, string_VkResult(result));
            }
            vkDestroyFence(device(), frame.fence, nullptr);
            frame.fence = VK_NULL_HANDLE;
        }
    }
    // Destroy command pool
    if (commandPool() != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device(), commandPool(), nullptr);
        commandPool() = VK_NULL_HANDLE;
    }

    // Destroy swapchain and surface
    pVkWindowDriver->destroy_resources();
    pVkWindowDriver->destroy_surface();

    ctx.destroy();

    m_swapchain = VK_NULL_HANDLE;

    // Destroy instance
    if (m_instance != VK_NULL_HANDLE)
    {
        if (all_windows_invisible())
        {
            delete m_queue;
            m_queue = nullptr;
            
            if (m_allocator != VK_NULL_HANDLE)
            {
                vmaDestroyAllocator(m_allocator);
                m_allocator = VK_NULL_HANDLE;
            }
            
            if (m_device != VK_NULL_HANDLE)
            {
                vkDestroyDevice(m_device, nullptr);
                m_device = VK_NULL_HANDLE;
            }
            
            vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
        }
    }
    
}


/**
  The destructor removes the widget and destroys the Vulkan context
  associated with it.
*/
Fl_Vk_Window::~Fl_Vk_Window()
{
    delete pVkWindowDriver;
}

void Fl_Vk_Window::wait_queue()
{
    if (queue() == VK_NULL_HANDLE)
        return;
    
    std::lock_guard<std::mutex> lock(queue_mutex());
    VkResult result = vkQueueWaitIdle(queue());
    VK_CHECK(result);
}

void Fl_Vk_Window::wait_device()
{
    if (device() == VK_NULL_HANDLE)
        return;
    VkResult result = vkDeviceWaitIdle(device());
    VK_CHECK(result);
}

void Fl_Vk_Window::init_colorspace()
{
    pVkWindowDriver->init_colorspace();
}

void Fl_Vk_Window::init_vulkan() {
    VkResult result;
    
    // Initialize Vulkan instance and device
    if (ctx.instance == VK_NULL_HANDLE) {
        pVkWindowDriver->init_vk();
        if (!ctx.instance)
        {
            fprintf(stderr, "init_vulkan() failed to create Vulkan instance\n");
            return;
        }
        pfn_vkCmdBeginDebugUtilsLabelEXT = 
            (PFN_vkCmdBeginDebugUtilsLabelEXT) vkGetInstanceProcAddr(instance(), "vkCmdBeginDebugUtilsLabelEXT");
        if (pfn_vkCmdBeginDebugUtilsLabelEXT == nullptr)
        {
            throw std::runtime_error("Failed to get vkCmdBeginDebugUtilsLabelEXT function pointer!");
        }

        pfn_vkCmdEndDebugUtilsLabelEXT = 
            (PFN_vkCmdEndDebugUtilsLabelEXT) vkGetInstanceProcAddr(instance(), "vkCmdEndDebugUtilsLabelEXT");
        if (pfn_vkCmdEndDebugUtilsLabelEXT == nullptr)
        {
            throw std::runtime_error("Failed to get vkCmdEndDebugUtilsLabelEXT function pointer!");
        }

        pfn_vkSetDebugUtilsObjectNameEXT =
            (PFN_vkSetDebugUtilsObjectNameEXT) vkGetInstanceProcAddr(instance(), "vkSetDebugUtilsObjectNameEXT");
        if (pfn_vkSetDebugUtilsObjectNameEXT == nullptr)
        {
            throw std::runtime_error("Failed to get vkSetDebugUtilsObjectNameEXT function pointer!");
        }
    }

    // Destroy surface
    if (m_surface != VK_NULL_HANDLE)
        pVkWindowDriver->destroy_surface();
        
    // Create surface
    pVkWindowDriver->create_surface();
    if (m_surface == VK_NULL_HANDLE) {
        fprintf(stderr, "Failed to create Vulkan's Window surface\n");
        return;
    }

    // Initialize colorspace
    init_colorspace();

    // Get the default function pointers after creating the VkDevice
    FLTK_ADD_DEVICE_EXTENSION(vkCmdSetColorWriteMaskEXT);
    
    FLTK_ADD_DEVICE_EXTENSION(vkCmdSetStencilTestEnableEXT);
    FLTK_ADD_DEVICE_EXTENSION(vkCmdSetStencilOpEXT);
    FLTK_ADD_DEVICE_EXTENSION(vkCmdSetStencilWriteMask);
    FLTK_ADD_DEVICE_EXTENSION(vkCmdSetStencilCompareMask);
    
    
    if (!vkSetHdrMetadataEXT)
    {
        bool found_hdr = false;
        for (auto extension : ctx.device_extensions)
        {
            if (strcmp(extension, VK_EXT_HDR_METADATA_EXTENSION_NAME) == 0)
            {
                found_hdr = true;
            }
        }

        if (found_hdr)
        {
            vkSetHdrMetadataEXT = (PFN_vkSetHdrMetadataEXT)vkGetDeviceProcAddr(device(), "vkSetHdrMetadataEXT");
            if (!vkSetHdrMetadataEXT)
            {
                fprintf(stderr, "Failed to initialize vkSetHDRMetadataEXT - no HDR extension will be used.\n");
            }
        }
    }

    // Verify window size
    if (w() <= 0 || h() <= 0) {
        fprintf(stderr, "Invalid window size: w=%d, h=%d\n", w(), h());
        pVkWindowDriver->destroy_surface();
        return;
    }

    // Create command pool
    VkCommandPoolCreateInfo cmd_pool_info = {};
    cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_info.queueFamilyIndex = ctx.queueFamilyIndex;
    cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    result = vkCreateCommandPool(device(), &cmd_pool_info, nullptr, &commandPool());
    if (result != VK_SUCCESS) {
        fprintf(stderr, "vkCreateCommandPool failed: %s\n", string_VkResult(result));
        wait_device();
        pVkWindowDriver->destroy_resources();
        pVkWindowDriver->destroy_surface();
        m_swapchain = VK_NULL_HANDLE;
        return;
    }
    
    // Initialize swapchain, views, depth/stencil and buffers
    pVkWindowDriver->prepare();
    if (m_swapchain == VK_NULL_HANDLE) {
        fprintf(stderr, "prepare() failed\n");
        wait_device();
        pVkWindowDriver->destroy_resources();
        pVkWindowDriver->destroy_surface();
        return;
    }

    // Get swapchain image count
    result = vkGetSwapchainImagesKHR(device(), m_swapchain, &m_swapchainImageCount, nullptr);
    if (result != VK_SUCCESS || m_swapchainImageCount == 0) {
        fprintf(stderr, "vkGetSwapchainImagesKHR failed: %s\n", string_VkResult(result));
        wait_device();
        pVkWindowDriver->destroy_resources();
        pVkWindowDriver->destroy_surface();
        m_swapchain = VK_NULL_HANDLE;
        return;
    }

    // Initialize frame data
    m_frames.resize(m_swapchainImageCount);
    VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT };
    VkCommandBufferAllocateInfo cmdInfo = {};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdInfo.commandPool = commandPool();
    cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdInfo.commandBufferCount = 1;

    for (auto& frame : m_frames) {
        result = vkCreateSemaphore(device(), &semaphoreInfo, nullptr, &frame.imageAcquiredSemaphore);
        if (result != VK_SUCCESS) {
            fprintf(stderr, "vkCreateSemaphore failed: %s\n", string_VkResult(result));
            shutdown_vulkan();
            return;
        }
        result = vkCreateFence(device(), &fenceInfo, nullptr, &frame.fence);
        if (result != VK_SUCCESS) {
            fprintf(stderr, "vkCreateFence failed: %s\n", string_VkResult(result));
            shutdown_vulkan();
            return;
        }
        result = vkAllocateCommandBuffers(device(), &cmdInfo, &frame.commandBuffer);
        if (result != VK_SUCCESS) {
            fprintf(stderr, "vkAllocateCommandBuffers failed: %s\n", string_VkResult(result));
            shutdown_vulkan();
            return;
        }
        frame.active = false;
    }

    m_currentFrameIndex = 0;
}


std::vector<const char*> Fl_Vk_Window::get_device_extensions()
{
    std::vector<const char*> out;

    // For easily changing stencil states
    out.push_back("VK_EXT_extended_dynamic_state");
    out.push_back("VK_EXT_extended_dynamic_state2");

    // For easily changing write masks
    out.push_back("VK_EXT_extended_dynamic_state3");
    return out;
}

std::vector<const char*> Fl_Vk_Window::get_instance_extensions()
{
    std::vector<const char*> out;
    out.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    return out;
}

std::vector<const char*> Fl_Vk_Window::get_optional_extensions()
{
    std::vector<const char*> out;
    out.push_back("VK_EXT_swapchain_colorspace");
    return out;
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
  
  // Reset Vulkan Handles
#ifdef NDEBUG
  m_validate = false;
  m_debugSync = false;
#else
  m_validate = true;
  m_debugSync = false;
#endif

  // Allocate a dummy safe thread queue
  // (will probably get overwritten by Fl_Vk_Window_Driver)
  if (m_queue == nullptr)
      m_queue = new Fl_Vk_Queue();
  ctx.safe_thread_queue = m_queue;
  
  m_surface = VK_NULL_HANDLE; // not really needed to keep in class

  // HDR metadata
  m_hdr_metadata = {};
  m_previous_hdr_metadata = {};
  m_hdr_metadata_changed = false;

  // Pointer to function to set HDR
  vkSetHdrMetadataEXT = nullptr;

  // Swapchain info
  m_pixels_per_unit = 0.F;
  m_swapchain = VK_NULL_HANDLE;

  // For drawing
  m_in_render_pass = false;
  m_clearColor = { 0.F, 0.F, 0.F, 0.F };
  m_depthStencil = 1.0;
  
  m_renderPass = VK_NULL_HANDLE;
  m_pipeline = VK_NULL_HANDLE;

  // Counters
  m_current_buffer = 0;
  m_last_presented_buffer = 0;
  m_currentFrameIndex = 0;
  m_swapchainImageCount = 0; // Track swapchain image count
}

/**
 \}
 \endcond
 */

#endif // HAVE_VK
