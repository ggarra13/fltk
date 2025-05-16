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

#ifndef Fl_Vk_Demos_H
#define Fl_Vk_Demos_H

#include <FL/vk.h>


struct Fl_Vk_Mesh {
    // Memory and Buffer used for the actual mesh
    VkBuffer buf;
    VkDeviceMemory mem;

    // Memory and Buffer used for indices (optional)
    VkDeviceMemory indexMem;
    VkBuffer indexBuffer;

    // Memory and Buffer used for UBO (matrix transforms)
    VkDeviceMemory uniformMemory;
    VkBuffer uniformBuffer;
    
    // Vertex bindings (hard coded for demos with two attributes only).
    VkVertexInputBindingDescription vi_bindings[1];
    VkVertexInputAttributeDescription vi_attrs[2];

    Fl_Vk_Mesh() :
        buf(VK_NULL_HANDLE),
        mem(VK_NULL_HANDLE),
        indexBuffer(VK_NULL_HANDLE),
        indexMem(VK_NULL_HANDLE),
        uniformBuffer(VK_NULL_HANDLE),
        uniformMemory(VK_NULL_HANDLE)
        {
        }

    void destroy(VkDevice& device)
        {
            if (buf != VK_NULL_HANDLE)
            {
                vkDestroyBuffer(device, buf, NULL);
                buf = VK_NULL_HANDLE;
            }

            if (mem != VK_NULL_HANDLE)
            {
                vkFreeMemory(device, mem, NULL);
                mem = VK_NULL_HANDLE;
            }
            
            if (indexBuffer != VK_NULL_HANDLE)
            {
                vkDestroyBuffer(device, indexBuffer, NULL);
                indexBuffer = VK_NULL_HANDLE;
            }

            if (indexMem != VK_NULL_HANDLE)
            {
                vkFreeMemory(device, indexMem, NULL);
                indexMem = VK_NULL_HANDLE;
            }
            
            if (uniformBuffer != VK_NULL_HANDLE)
            {
                vkDestroyBuffer(device, uniformBuffer, NULL);
                uniformBuffer = VK_NULL_HANDLE;
            }

            if (uniformMemory != VK_NULL_HANDLE)
            {
                vkFreeMemory(device, uniformMemory, NULL);
                uniformMemory = VK_NULL_HANDLE;
            }
        }
};

  
struct Fl_Vk_Texture {
    VkImageType type;           // VK_IMAGE_TYPE_1D, 2D or 3D

    VkFormat format;
    
    VkImage image;              // Vulkan image handle
    VkImageView view;           // View for shader access
    VkSampler sampler;          // Sampler for texture sampling

    uint32_t    binding;
    std::string samplerName;        // name of sampler

    VkDeviceMemory mem;             // Allocated memory for texture
    uint32_t width, height, depth;  // texture width/height/depth

    Fl_Vk_Texture() :
        type(VK_IMAGE_TYPE_2D),
        format(VK_FORMAT_B8G8R8A8_UNORM),
        image(VK_NULL_HANDLE),
        view(VK_NULL_HANDLE),
        sampler(VK_NULL_HANDLE),
        mem(VK_NULL_HANDLE),
        samplerName("sampler1"),
        binding(0),
        width(1),
        height(1),
        depth(1)
        {
        }
    
    Fl_Vk_Texture(VkImageType type,
                  VkFormat    format,
                  VkImage     image,
                  VkImageView view,
                  VkSampler sampler,
                  VkDeviceMemory mem,
                  std::string samplerName = "sampler1",
                  uint32_t binding = 0,
                  uint32_t width = 1,
                  uint32_t height = 1,
                  uint32_t depth = 1) :
        type(type),
        format(format),
        image(image),
        view(view),
        sampler(sampler),
        mem(mem),
        samplerName(samplerName),
        binding(binding),
        width(width),
        height(height),
        depth(depth)
        {
        }

    void destroy(VkDevice& device)
        {
            if (sampler != VK_NULL_HANDLE)
            {
                vkDestroySampler(device, sampler, NULL);
                sampler = VK_NULL_HANDLE;
            }
            if (view != VK_NULL_HANDLE)
            {
                vkDestroyImageView(device, view, NULL);
                view = VK_NULL_HANDLE;
            }
            if (mem != VK_NULL_HANDLE)
            {
                vkFreeMemory(device, mem, NULL);
                mem = VK_NULL_HANDLE;
            }
            if (image != VK_NULL_HANDLE)
            {
                vkDestroyImage(device, image, NULL);
                image = VK_NULL_HANDLE;
            }
        }
};

  

#endif // Fl_Vk_Demos_H
