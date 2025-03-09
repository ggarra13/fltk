//
// Line style code for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2017 by Bill Spitzak and others.
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
  \file Fl_Vulkan_Graphics_Driver_line_style.cxx
  \brief Line style drawing utility hiding different platforms.
*/

#include <config.h>
#include "Fl_Vulkan_Graphics_Driver.H"
#include <FL/vk.h>
#include <FL/Fl_Vk_Window.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl.H>
#include <FL/fl_draw.H>

// Vulkan implementation does not support custom patterns
// Vulkan implementation does not support cap and join types

void Fl_Vulkan_Graphics_Driver::line_style(int style, int width, char* dashes) {
  if (width<1) width = 1;
  line_width_ = (float)width;

  int stipple = style & 0x00ff;
  line_stipple_ = stipple;
//  int cap     = style & 0x0f00;
//  int join    = style & 0xf000;

  if (stipple==FL_SOLID) {
    vkLineStipple(1, 0xFFFF);
    vkDisable(VK_LINE_STIPPLE);
  } else {
    char enable = 1;
    switch (stipple & 0x00ff) {
      case FL_DASH:
        vkLineStipple(VKint(pixels_per_unit_*line_width_), 0x0F0F); // ....****....****
        break;
      case FL_DOT:
        vkLineStipple(VKint(pixels_per_unit_*line_width_), 0x5555); // .*.*.*.*.*.*.*.*
        break;
      case FL_DASHDOT:
        vkLineStipple(VKint(pixels_per_unit_*line_width_), 0x2727); // ..*..***..*..***
        break;
      case FL_DASHDOTDOT:
        vkLineStipple(VKint(pixels_per_unit_*line_width_), 0x5757); // .*.*.***.*.*.***
        break;
      default:
        vkLineStipple(1, 0xFFFF);
        enable = 0;
    }
    if (enable)
      vkEnable(VK_LINE_STIPPLE);
    else
      vkDisable(VK_LINE_STIPPLE);
  }
  vkLineWidth( (VKfloat)(pixels_per_unit_ * line_width_) );
  vkPointSize( (VKfloat)(pixels_per_unit_) );
}
