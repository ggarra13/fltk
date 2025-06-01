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
   Fl_Vk_Context struct . */

#ifndef Fl_Vk_Context_H
#define Fl_Vk_Context_H

#include "FL/vk.h"

#include <mutex>
#include <vector>

//! Wrapper to keep a queue and a mutex for safe use in multithreading.
//! Each FLTK window receives a pointer to it.
struct Fl_Vk_Queue
{
    std::mutex                    mutex;
    VkQueue                       queue;

    Fl_Vk_Queue() :
        queue(VK_NULL_HANDLE)
        {
        }
};

// We keep this in a context struct, so we can pass them to some other library
// or functions easily.
struct Fl_Vk_Context
{
    VkInstance         instance;   // created once and filled later from Fl_Vk_Window_Driver

    VkPhysicalDevice   gpu;
    VkPhysicalDeviceProperties gpu_props;
    VkPhysicalDeviceFeatures gpu_features;
    
    VkDevice           device;   // created once and filled later from Fl_Vk_Window_Driver
    VkPipelineCache    pipelineCache; 

    VkQueueFamilyProperties* queue_props;
    Fl_Vk_Queue*      safe_thread_queue;
    uint32_t                queue_count;
    uint32_t           queueFamilyIndex;
    
    VkCommandPool           commandPool;  // memory for command buffers

    VkFormat                     format;   // for depth and color formats
    VkColorSpaceKHR          colorSpace; // sRGB, HDR10 or HLG for example

    VmaAllocator             allocator;
    
    // List of enabled layers
    std::vector<const char*> enabled_layers;

    // List of instance extensions
    std::vector<const char*> instance_extensions;

    // List of device extensions
    std::vector<const char*> device_extensions;

    VkQueue& queue() { return safe_thread_queue->queue; }
    std::mutex& queue_mutex() { return safe_thread_queue->mutex; }
    
    Fl_Vk_Context() :
        instance(VK_NULL_HANDLE),
        gpu(VK_NULL_HANDLE),
        device(VK_NULL_HANDLE),
        pipelineCache(VK_NULL_HANDLE),
        queue_props(nullptr),
        safe_thread_queue(nullptr),
        queue_count(0),
        commandPool(VK_NULL_HANDLE),
        allocator(VK_NULL_HANDLE)
        {
        };

    void destroy()
        {
            VkResult result = vkDeviceWaitIdle(device);
            VK_CHECK(result);
            
            if (pipelineCache != VK_NULL_HANDLE)
            {
                vkDestroyPipelineCache(device, pipelineCache, nullptr);
                pipelineCache = VK_NULL_HANDLE;
            }
            if (commandPool != VK_NULL_HANDLE)
            {
                vkDestroyCommandPool(device, commandPool, nullptr);
                commandPool = VK_NULL_HANDLE;
            }

            // Queue, device and allocator are statics shared across all windows.
            // They are destroyed on program exit.
        }
};

#endif // Fl_Vk_Context_H
