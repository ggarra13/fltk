//
// Vulkan definitions for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2018 by Bill Spitzak and others.
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

// Internal interface to set up Vulkan.
//
// A "Fl_Vk_Choice" is created from a Vulkan mode and holds information
// to create a Vulkan "context" (on both Wayland, macOS and Win32).
//
// create_vk_context takes a window (necessary only on Win32) and an
// Fl_Vk_Choice and returns a new Vulkan context. All contexts share
// display lists with each other.
//
// This code is used by Fl_Vk_Window, vk_start(), and vk_visual()

#ifndef Fl_Vk_Choice_H
#define Fl_Vk_Choice_H

#include "vulkan/vulkan.h"

// Describes the platform-independent part of data needed to create a VKContext.
class Fl_Vk_Choice {
  friend class Fl_Vk_Window_Driver;

protected:
  // Vulkan objects
  VkInstance instance;
  VkPhysicalDevice physicalDevice;
  VkDevice device;
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
  VkQueue queue;

  int mode;
  const int *alist;
  Fl_Vk_Choice *next;

public:
  Fl_Vk_Choice(int m, const int *alistp, Fl_Vk_Choice *n)
    : mode(m)
    , alist(alistp)
    , next(n) {}
};

#endif // Fl_Vk_Choice_H
