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

#include <iostream>  // \@todo: remove
#include <FL/platform.H>

#  include "Enumerations.H" // for color names
#  ifdef _WIN32
#    include <windows.h>
#  endif
#  ifndef APIENTRY
#    if defined(__CYGWIN__)
#      define APIENTRY __attribute__ ((__stdcall__))
#    else
#      define APIENTRY
#    endif
#  endif

FL_EXPORT void vk_start();
FL_EXPORT void vk_finish();

FL_EXPORT void vk_color(Fl_Color i);
/** back compatibility */
inline void vk_color(int c) {vk_color((Fl_Color)c);}

FL_EXPORT void vk_rect(int x,int y,int w,int h);
FL_EXPORT void vk_rectf(int x,int y,int w,int h);

FL_EXPORT void vk_font(int fontid, int size);
FL_EXPORT int  vk_height();
FL_EXPORT int  vk_descent();
FL_EXPORT double vk_width(const char *);
FL_EXPORT double vk_width(const char *, int n);
FL_EXPORT double vk_width(uchar);

FL_EXPORT void vk_draw(const char*);
FL_EXPORT void vk_draw(const char*, int n);
FL_EXPORT void vk_draw(const char*, int x, int y);
FL_EXPORT void vk_draw(const char*, float x, float y);
FL_EXPORT void vk_draw(const char*, int n, int x, int y);
FL_EXPORT void vk_draw(const char*, int n, float x, float y);
FL_EXPORT void vk_draw(const char*, int x, int y, int w, int h, Fl_Align);
FL_EXPORT void vk_measure(const char*, int& x, int& y);
FL_EXPORT void vk_texture_pile_height(int max);
FL_EXPORT int  vk_texture_pile_height();
FL_EXPORT void vk_texture_reset();

FL_EXPORT void vk_draw_image(const uchar *, int x,int y,int w,int h, int d=3, int ld=0);

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

#define VK_CHECK_RESULT(err) vk_check_result(err, __FILE__, __LINE__);
#define VK_CHECK_HANDLE(var) vk_check_handle(var == VK_NULL_HANDLE, \
                                             #var,  __FILE__, __LINE__);

#define VK_ALLOC(x)  (void*)malloc(x)
#define VK_ASSERT(x) assert(x)
#define VK_UNUSED(x) (void)(x)
#define VK_FREE(x) free(x);
#define VK_ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define VK_DBG() std::cerr << __FILE__ << " " << __FUNCTION__ \
                           << " " << __LINE__ << std::endl;


inline void vk_check_handle(bool is_null_handle,
                            const char* var, const char* file, const int line)
{
    if (is_null_handle)
    {
        char buf[256];
        snprintf(buf, 256, "Vulkan: VK_NULL_HANDLE for %s in %s, line %d",
                   var, file, line);
        Fl::fatal(buf);
        exit(1);
      }
}

inline void vk_check_result(VkResult err, const char* file, const int line)
{
    const char* errorName = "Unknown Error";
    switch(err)
    {
    case VK_SUCCESS:
        return;
    case VK_NOT_READY:
        errorName = "Not Ready";
        break;
    case VK_TIMEOUT:
        errorName = "Timeout";
        break;
    case VK_EVENT_SET:
        errorName = "Event Set";
        break;
    case VK_EVENT_RESET:
        errorName = "Event Reset";
        break;
    case VK_INCOMPLETE:
        errorName = "Incomplete";
        break;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        errorName = "Error - Out of Host Memory";
        break;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        errorName = "Error - Out of Device Memory";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        errorName = "Error - Initialization Failed";
        break;
    case VK_ERROR_DEVICE_LOST:
        errorName = "Error - Device Lost";
        break;
    case VK_ERROR_MEMORY_MAP_FAILED:
        errorName = "Error - Memory Map Failed";
        break;
    case VK_ERROR_LAYER_NOT_PRESENT:
        errorName = "Error - Layer Not Present";
        break;
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        errorName = "Error - Extension Not Present";
        break;
    case VK_ERROR_FEATURE_NOT_PRESENT:
        errorName = "Error - Feature Not Present";
        break;
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        errorName = "Error - Incompatible Driver";
        break;
    case VK_ERROR_TOO_MANY_OBJECTS:
        errorName = "Error - Too Many Objects";
        break;
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        errorName = "Error - Format Not Supported";
        break;
    case VK_ERROR_FRAGMENTED_POOL:
        errorName = "Error - Fragmented Pool";
        break;
    default:
        break;
    }
    char buf[256];
    snprintf(buf, 256, "Vulkan: %s (%u) at %s, line %d", errorName, err, file, line);
    Fl::error(buf);
    exit(err);
}

#endif // !FL_vk_H
