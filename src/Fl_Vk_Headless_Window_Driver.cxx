//
// Class Fl_Vk_Headless_Window_Driver for the Fast Light Tool Kit (FLTK).
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

#include "Fl_Vk_Headless_Window_Driver.H"
#include <FL/vk_enum_string_helper.h>
#include <FL/Fl_Vk_Window.H>
#include <FL/Fl_Vk_Utils.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl.H>

// Reads pixels directly out of the current offscreen VkImage via a
// host-visible staging buffer. There is no OS window to screenshot (that's
// what the interactive drivers' overrides of this function do instead), so
// this has to go through Vulkan itself.
//
// Assumes the app's render pass leaves the color attachment in
// VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL (its finalLayout) when
// is_headless() is true -- e.g. instead of VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
// which only makes sense when there's a real presentation engine to hand
// the image to. See vk_shape.cxx-style demos for the render-pass side of
// this contract.
Fl_RGB_Image *Fl_Vk_Headless_Window_Driver::capture_vk_rectangle(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0 || pWindow->empty_buffers())
    return NULL;

  const VkDeviceSize bufSize = (VkDeviceSize)w * h * 4; // 4 bytes/pixel source format

  VkBuffer stagingBuf;
  VkDeviceMemory stagingMem;
  createBuffer(device(), gpu(), bufSize,
              VK_BUFFER_USAGE_TRANSFER_DST_BIT,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
              stagingBuf, stagingMem);

  VkCommandBuffer cmd = beginSingleTimeCommands(device(), pWindow->commandPool());

  VkImage src = pWindow->get_back_buffer_image(); // public accessor -- see note above

  VkBufferImageCopy region = {};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;   // tightly packed
  region.bufferImageHeight = 0; // tightly packed
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = { x, y, 0 };
  region.imageExtent = { (uint32_t)w, (uint32_t)h, 1 };

  vkCmdCopyImageToBuffer(cmd, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         stagingBuf, 1, &region);

  // Waits on the queue internally -- no extra synchronization needed here.
  endSingleTimeCommands(cmd, device(), pWindow->commandPool(), queue());

  void *mapped = nullptr;
  VkResult result = vkMapMemory(device(), stagingMem, 0, bufSize, 0, &mapped);
  if (result != VK_SUCCESS) {
    fprintf(stderr, "capture_vk_rectangle: vkMapMemory failed: %s\n", string_VkResult(result));
    vkDestroyBuffer(device(), stagingBuf, nullptr);
    vkFreeMemory(device(), stagingMem, nullptr);
    return NULL;
  }

  // Convert tightly-packed 4-byte source pixels to the 3-byte RGB buffer
  // Fl_RGB_Image expects. pWindow->format() is chosen in
  // Fl_Vk_Window_Driver::init_colorspace()'s headless branch
  // (VK_FORMAT_R8G8B8A8_UNORM by default) -- if a driver subclass overrides
  // that to a BGRA format, swap the swizzle below accordingly.
  uchar *pixels = new uchar[(size_t)w * h * 3];
  const uchar *src_bytes = (const uchar *)mapped;
  for (int i = 0; i < w * h; ++i) {
    const uchar *s = src_bytes + (size_t)i * 4;
    pixels[i * 3 + 0] = s[0]; // R
    pixels[i * 3 + 1] = s[1]; // G
    pixels[i * 3 + 2] = s[2]; // B
  }

  vkUnmapMemory(device(), stagingMem);
  vkDestroyBuffer(device(), stagingBuf, nullptr);
  vkFreeMemory(device(), stagingMem, nullptr);

  Fl_RGB_Image *img = new Fl_RGB_Image(pixels, w, h, 3);
  img->alloc_array = 1;
  return img;
}

#endif // HAVE_VK
