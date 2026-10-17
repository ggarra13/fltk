//
// Vulkan header file for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2018 by Bill Spitzak and others.
//
// You must include this instead of GL/gl.h to get the Microsoft
// APIENTRY stuff included (from <windows.h>) prior to the Vulkan
// header files.
//
// This file also provides "missing" Vulkan functions, and
// gl_start() and gl_finish() to allow Vulkan to be used in any window
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

/**
 \file vk.h
 This file defines wrapper functions for Vulkan in FLTK

 To use Vulkan from within an FLTK application you MUST use vk_visual()
 to select the default visual before doing show() on any windows. Mesa
 will crash if you try to use a visual not returned by vkxChooseVisual.

 Historically, this did not always work well with Fl_Double_Window's!
 It can try to draw into the front buffer.
 Depending on the system this might either
 crash or do nothing (when pixmaps are being used as back buffer
 and VK is being done by hardware), work correctly (when VK is done
 with software, such as Mesa), or draw into the front buffer and
 be erased when the buffers are swapped (when double buffer hardware
 is being used)
 */

#ifndef FL_vk_H
#  define FL_vk_H

#include "Fl_Export.H"

#  ifdef _WIN32
#    define NOMINMAX
#    include <windows.h>
#  endif
#  ifndef APIENTRY
#    if defined(__CYGWIN__)
#      define APIENTRY __attribute__ ((__stdcall__))
#    else
#      define APIENTRY
#    endif
#  endif

#ifdef _WIN32
#  define VK_USE_PLATFORM_WIN32_KHR
#  include <vulkan/vulkan.h>
#elif defined(__linux__)
#  ifdef FLTK_USE_X11
#    define VK_USE_PLATFORM_XLIB_KHR
#  endif
#  ifdef FLTK_USE_WAYLAND
#    define VK_USE_PLATFORM_WAYLAND_KHR
#  endif
#  include <vulkan/vulkan.h>
#else
#  include <vulkan/vulkan.h>
#endif

#define VMA_CALL_PRE FL_EXPORT

#define VMA_VULKAN_VERSION 1002000 // Vulkan 1.2
#include "FL/vk_mem_alloc.h"

#define VK_CHECK(err) vk_check_result(err, __FILE__, __LINE__);

#define VK_ALLOC(x)  (void*)malloc(x)
#define VK_ASSERT(x) assert(x)
#define VK_UNUSED(x) (void)(x)
#define VK_FREE(x) free(x);
#define VK_ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define VK_DBG() std::cerr << __FILE__ << " " << __FUNCTION__ \
                           << " " << __LINE__ << std::endl;
    
FL_EXPORT void vk_check_result(VkResult err, const char* file, const int line);


extern PFN_vkCmdBeginDebugUtilsLabelEXT fltk_vkCmdBeginDebugUtilsLabelEXT;
extern PFN_vkCmdEndDebugUtilsLabelEXT fltk_vkCmdEndDebugUtilsLabelEXT;
extern PFN_vkSetDebugUtilsObjectNameEXT fltk_vkSetDebugUtilsObjectNameEXT;
extern PFN_vkCreateDebugUtilsMessengerEXT fltk_vkCreateDebugUtilsMessengerEXT;
extern PFN_vkDestroyDebugUtilsMessengerEXT fltk_vkDestroyDebugUtilsMessengerEXT;
extern PFN_vkCmdInsertDebugUtilsLabelEXT fltk_vkCmdInsertDebugUtilsLabelEXT;


/** 
 * Name a Vulkan object for debugging.
 * 
 * @param object_type   type of object, like:
 *                              VK_OBJECT_TYPE_COMMAND_BUFFER,
 *                              VK_OBJECT_TYPE_BUFFER,
 *                              ...etc...
 *                              (see vulkan/vulkan_core.h's VkObjectType)
 *                              
 * @param object_handle Vulkan Handle cast to uint64_t
 * @param name          Debugging name.
 */
FL_EXPORT void vk_set_object_name(VkDevice,
                                  VkObjectType object_type,
                                  uint64_t object_handle,
                                  const char* name);

  /** 
   * Begin a debug label in a command buffer with a given color
   * 
   * @param cmd          Vulkan Command Buffer
   * @param label_name   Name for the label. 
   * @param r            red color
   * @param g            green color
   * @param b            blue color
   * @param a            alpha color
   */
FL_EXPORT void vk_begin_debug_label(VkCommandBuffer cmd, const char* label_name,
                                    const float r = 1.F, const float g = 0.5F,
                                    const float b = 0.F, const float a = 1.F);
FL_EXPORT void vk_end_debug_label(VkCommandBuffer cmd);
#endif // !FL_vk_H
