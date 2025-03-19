//
// Arc (integer) drawing functions for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2010 by Bill Spitzak and others.
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
  \file Fl_Vulkan_Graphics_Driver_arci.cxx
  \brief Utility functions for drawing circles using integers
*/

#include <config.h>
#include "Fl_Vulkan_Graphics_Driver.H"
#include <FL/vk.h>
#include <FL/Fl_Vk_Window.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl.H>
#include <FL/fl_draw.H>
#define _USE_MATH_DEFINES
#include <FL/math.h>

void Fl_Vulkan_Graphics_Driver::arc(int x, int y, int w, int h, double a1, double a2) {
  // if (w <= 0 || h <= 0) return;
  // while (a2<a1) a2 += 360.0;  // TODO: write a sensible fmod anvke alignment here
  // a1 = a1/180.0*M_PI; a2 = a2/180.0*M_PI;
  // double cx = x + 0.5*w, cy = y + 0.5*h;
  // double rx = 0.5*w-0.3, ry = 0.5*h-0.3;
  // double rMax; if (w>h) rMax = rx; else rMax = ry;
  // int nSeg = (int)(10 * sqrt(rMax))+1;
  // double incr = (a2-a1)/(double)nSeg;

  // vkBegin(VK_LINE_STRIP);
  // for (int i=0; i<=nSeg; i++) {
  //   vkVertex2d(cx+cos(a1)*rx, cy-sin(a1)*ry);
  //   a1 += incr;
  // }
  // vkEnd();
}

void Fl_Vulkan_Graphics_Driver::arc(double x, double y, double r, double start, double end) {
  Fl_Graphics_Driver::arc(x, y, r, start, end);
}

void Fl_Vulkan_Graphics_Driver::pie(int x, int y, int w, int h, double a1, double a2) {
  // if (w <= 0 || h <= 0) return;
  // while (a2<a1) a2 += 360.0;  // TODO: write a sensible fmod anvke alignment here
  // a1 = a1/180.0*M_PI; a2 = a2/180.0*M_PI;
  // double cx = x + 0.5*w, cy = y + 0.5*h;
  // double rx = 0.5*w, ry = 0.5*h;
  // double rMax; if (w>h) rMax = rx; else rMax = ry;
  // int nSeg = (int)(10 * sqrt(rMax))+1;
  // double incr = (a2-a1)/(double)nSeg;

  // vkBegin(VK_TRIANVKE_FAN);
  // vkVertex2d(cx, cy);
  // for (int i=0; i<=nSeg; i++) {
  //   vkVertex2d(cx+cos(a1)*rx, cy-sin(a1)*ry);
  //   a1 += incr;
  // }
  // vkEnd();
}
