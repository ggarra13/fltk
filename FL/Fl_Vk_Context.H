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
//! members can be copied across field-for-field when filling that struct.
//!   idx        - queue family index (VkDeviceQueueCreateInfo.queueFamilyIndex)
//!   num        - number of queues from this family actually created on the
//!                device (i.e. how many are safe to request via vkGetDeviceQueue
//!                with queueIndex in [0, num))
//!   flags      - the family's full VkQueueFlags (may include GRAPHICS/COMPUTE/
//!                TRANSFER in addition to VIDEO_DECODE_BIT_KHR)
//!   video_caps - codec operations this family supports (H.264, HEVC, AV1, ...),
//!                from VkQueueFamilyVideoPropertiesKHR::videoCodecOperations
struct Fl_Vk_Video_Queue_Family
{
    int32_t                       idx;
    int32_t                       num;
    VkQueueFlags                  flags;
    VkVideoCodecOperationFlagsKHR video_caps;

    Fl_Vk_Video_Queue_Family() :
        idx(-1),
        num(0),
        flags(0),
        video_caps(0)
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

    // Queue families exposing VK_QUEUE_VIDEO_DECODE_BIT_KHR, discovered
    // alongside queue_props above. Usually has zero or one entry, but a GPU
    // is free to expose several video-capable families that differ in
    // video_caps (e.g. one for H.264/HEVC, another for AV1). Empty unless
    // VK_KHR_video_queue/VK_KHR_video_decode_queue are supported and enabled.
    std::vector<Fl_Vk_Video_Queue_Family> video_decode_queue_families;

    // Thread-safe handle for the queue created from the *first* entry of
    // video_decode_queue_families (nullptr if none was created). Mirrors
    // safe_thread_queue above.
    Fl_Vk_Queue*       safe_thread_video_decode_queue;

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

    // !@{
    //    Function pointers to useful extensitons

    // These allow color write masks in Vulkan 1.3
    PFN_vkCmdSetColorWriteMaskEXT vkCmdSetColorWriteMaskEXT;

    // These allow changing stencil masks/tests
    PFN_vkCmdSetStencilTestEnableEXT vkCmdSetStencilTestEnableEXT;
    PFN_vkCmdSetStencilOpEXT vkCmdSetStencilOpEXT;
    PFN_vkCmdSetStencilWriteMask vkCmdSetStencilWriteMask;
    PFN_vkCmdSetStencilCompareMask vkCmdSetStencilCompareMask;
    // !@}

    // Thread safe queue (use lock_guard on mutex when submitting)
    VkQueue& queue() { return safe_thread_queue->queue; }
    std::mutex& queue_mutex() { return safe_thread_queue->mutex; }

    // Thread safe video decode queue. Only valid (non-null) if
    // video_decode_queue_families is non-empty.
    bool has_video_decode_queue() const { return safe_thread_video_decode_queue != nullptr; }
    VkQueue& video_decode_queue() { return safe_thread_video_decode_queue->queue; }
    std::mutex& video_decode_queue_mutex() { return safe_thread_video_decode_queue->mutex; }

    Fl_Vk_Context() :
        instance(VK_NULL_HANDLE),
        gpu(VK_NULL_HANDLE),
        device(VK_NULL_HANDLE),
        pipelineCache(VK_NULL_HANDLE),
        queue_props(nullptr),
        safe_thread_queue(nullptr),
        queue_count(0),
        safe_thread_video_decode_queue(nullptr),
        commandPool(VK_NULL_HANDLE),
        allocator(VK_NULL_HANDLE),
        vkCmdSetColorWriteMaskEXT(nullptr),
        vkCmdSetStencilTestEnableEXT(nullptr),
        vkCmdSetStencilOpEXT(nullptr),
        vkCmdSetStencilWriteMask(nullptr),
        vkCmdSetStencilCompareMask(nullptr)
        {
        };

    void destroy()
        {
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

            // If the video decode queue got its own dedicated Fl_Vk_Queue
            // (i.e. it isn't just an alias of the shared graphics queue
            // wrapper), it's owned by this context and must be freed here.
            if (safe_thread_video_decode_queue != nullptr &&
                safe_thread_video_decode_queue != safe_thread_queue)
            {
                delete safe_thread_video_decode_queue;
            }
            safe_thread_video_decode_queue = nullptr;

            // Queue, device and allocator are statics shared across all windows.
            // They are destroyed on program exit.
        }
};

#endif // Fl_Vk_Context_H
