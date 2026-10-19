//
// Implementation of class Fl_Vk_Window_Driver, and of its platform-specific derived classes
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

#include "Fl_WebGPU_Window_Driver.H"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <vector>


// Recreates m_swapchain and m_buffers
void Fl_WebGPU_Window_Driver::prepare_buffers() {
}

// Prepare depth/stencil resources for window
void Fl_WebGPU_Window_Driver::prepare_depth() {
    // bool has_depth   = pWindow->mode() & FL_DEPTH;
    // bool has_stencil = pWindow->mode() & FL_STENCIL;

    // if (!has_depth && !has_stencil) {
    //     // Leave depth zeroed; m_currentDepthLayout stays UNDEFINED.
    //     return;
    // }

    // VkFormat   depth_format = VK_FORMAT_D16_UNORM;
    // uint32_t   aspectMask   = VK_IMAGE_ASPECT_DEPTH_BIT;

    // if (has_stencil) {
    //     if (has_depth) {
    //         depth_format = VK_FORMAT_D24_UNORM_S8_UINT;
    //         aspectMask   = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    //     } else {
    //         depth_format = VK_FORMAT_S8_UINT;
    //         aspectMask   = VK_IMAGE_ASPECT_STENCIL_BIT;
    //     }
    // }

    // // Use the already-resolved swapchain extent (no extra driver query needed).
    // const VkExtent2D& ext = pWindow->m_swapchainExtent;

    // // --- VkImageCreateInfo is unchanged from the original ---
    // VkImageCreateInfo image_info = {};
    // image_info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    // image_info.imageType     = VK_IMAGE_TYPE_2D;
    // image_info.format        = depth_format;
    // image_info.extent        = { ext.width, ext.height, 1 };
    // image_info.mipLevels     = 1;
    // image_info.arrayLayers   = 1;
    // image_info.samples       = VK_SAMPLE_COUNT_1_BIT;
    // image_info.tiling        = VK_IMAGE_TILING_OPTIMAL;
    // image_info.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    // image_info.flags         = 0;

    // VmaAllocationCreateInfo alloc_info = {};
    // alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    // // Prefer dedicated allocation for depth images — drivers often benefit from
    // // this hint for large render targets (it avoids sub-allocating inside a
    // // shared heap block, reducing aliasing risk).
    // alloc_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    // pWindow->m_depth.format = depth_format;

    // // Single call replaces vkCreateImage + vkGetImageMemoryRequirements +
    // // vkAllocateMemory + vkBindImageMemory.
    // VkResult result = vmaCreateImage(
    //     pWindow->m_allocator,
    //     &image_info,
    //     &alloc_info,
    //     &pWindow->m_depth.image,
    //     &pWindow->m_depth.allocation,
    //     nullptr
    // );

    // if (result != VK_SUCCESS) {
    //     fprintf(stderr, "vmaCreateImage (depth) failed: %s\n", string_VkResult(result));
    //     return;
    // }

    // // Image view creation is unchanged.
    // VkImageSubresourceRange subresourceRange = {};
    // subresourceRange.aspectMask     = aspectMask;
    // subresourceRange.baseMipLevel   = 0;
    // subresourceRange.levelCount     = 1;
    // subresourceRange.baseArrayLayer = 0;
    // subresourceRange.layerCount     = 1;

    // VkImageViewCreateInfo view_info = {};
    // view_info.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    // view_info.image            = pWindow->m_depth.image;
    // view_info.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    // view_info.format           = depth_format;
    // view_info.subresourceRange = subresourceRange;

    // result = vkCreateImageView(pWindow->device(), &view_info, nullptr,
    //                            &pWindow->m_depth.view);
    // VK_CHECK(result);

    // pWindow->m_currentDepthLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}


// Uses m_renderPass
void Fl_WebGPU_Window_Driver::prepare_framebuffers() {
  // VkImageView attachments[2];
  // attachments[0] = VK_NULL_HANDLE;        // Color attachment
  // attachments[1] = pWindow->m_depth.view; // Depth/stencil (optional)

  // uint32_t W = pWindow->m_swapchainExtent.width;
  // uint32_t H = pWindow->m_swapchainExtent.height;

  // VkFramebufferCreateInfo fb_info = {};
  // fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  // fb_info.pNext = NULL;
  // fb_info.renderPass = pWindow->m_renderPass;
  // bool has_depth = pWindow->mode() & FL_DEPTH;
  // bool has_stencil = pWindow->mode() & FL_STENCIL;
  // fb_info.attachmentCount = (has_depth || has_stencil) ? 2 : 1;
  // fb_info.pAttachments = attachments;
  // fb_info.width = W;
  // fb_info.height = H;
  // fb_info.layers = 1;

  // VkResult result;
  // for (auto& buffer : pWindow->m_buffers)
  // {
  //     attachments[0] = buffer.view;
  //     result = vkCreateFramebuffer(pWindow->device(), &fb_info, NULL, &buffer.framebuffer);
  //     VK_CHECK(result);
  // }
}

void Fl_WebGPU_Window_Driver::init_instance()
{
}

void Fl_WebGPU_Window_Driver::init_vk(int requested_device_index)
{

}

void Fl_WebGPU_Window_Driver::create_device()
{
}

void Fl_WebGPU_Window_Driver::prepare() {
    int W = pWindow->pixel_w();
    int H = pWindow->pixel_h();
    resize(true, W, H);

    prepare_buffers();
    prepare_depth();
    pWindow->prepare();
    prepare_framebuffers(); // can be kept in driver
}

void Fl_WebGPU_Window_Driver::destroy_surface() {
}

// Uses: device(), m_buffers, m_depth
void Fl_WebGPU_Window_Driver::destroy_resources()
{
    if (!pWindow || !pWindow->wgpu.device)
    {
        fprintf(stderr, "destroy_resources: Invalid device\n");
        return;
    }

    // Destroy resources in reverse creation order (first, those of window)
    pWindow->destroy_common_resources();

    // Then, depth/stencils if present
    //pWindow->m_depth.destroy(pWindow->device(), pWindow->m_allocator);
}


Fl_WebGPU_Window_Driver::Fl_WebGPU_Window_Driver(Fl_WebGPU_Window* win) :
    pWindow(win)
{
    swap_interval_ = 0;  // MAILBOX as a default swapchain presentation mode
}

Fl_WebGPU_Window_Driver::~Fl_WebGPU_Window_Driver()
{
}
