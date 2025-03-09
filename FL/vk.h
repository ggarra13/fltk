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

#include <vulkan/vulkan.h>

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

#endif // !FL_vk_H
