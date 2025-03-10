//
// Class Fl_WinAPI_Vk_Window_Driver for the Fast Light Tool Kit (FLTK).
//
// Copyright 2021-2022 by Bill Spitzak and others.
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
#include <FL/platform.H>
#include "../../Fl_Screen_Driver.H"
#include <FL/vk.h>
#include "Fl_WinAPI_Vk_Window_Driver.H"
#include "../../Fl_Vk_Choice.H"
#include "Fl_WinAPI_Window_Driver.H"
#include "../GDI/Fl_Font.H"
extern void fl_save_dc(HWND, HDC);


// Describes crap needed to create a VKContext.
class Fl_WinAPI_Vk_Choice : public Fl_Vk_Choice {
  friend class Fl_WinAPI_Vk_Window_Driver;
private:
public:
  Fl_WinAPI_Vk_Choice(int m, const int *alistp, Fl_Vk_Choice *n) : Fl_Vk_Choice(m, alistp, n)
        {
            // Get surface capabilities
            VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = {};
            surfaceInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
            surfaceInfo.surface = surface;

            VkSurfaceCapabilities2KHR surfaceCaps = {};
            surfaceCaps.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;

            VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo2 = surfaceInfo;
            surfaceInfo2.pNext = &surfaceCaps;

            vkGetPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &surfaceInfo2,
                                                       &surfaceCaps);

            // Get surface formats
            uint32_t formatCount = 0;
            vkGetPhysicalDeviceSurfaceFormats2KHR(physicalDevice, &surfaceInfo,
                                                  &formatCount, nullptr);
            std::vector<VkSurfaceFormat2KHR> surfaceFormats(formatCount);

            vkGetPhysicalDeviceSurfaceFormats2KHR(physicalDevice, &surfaceInfo,
                                                  &formatCount, surfaceFormats.data());

            // Get present modes
            uint32_t presentModeCount = 0;
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                                      &presentModeCount, nullptr);
            std::vector<VkPresentModeKHR> presentModes(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                                      &presentModeCount, presentModes.data());

            // Filter formats and present modes
            for (const auto& format : surfaceFormats) {
                if (format.surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM)
                {
                    // Found a suitable format
                }
            }

            for (const auto& presentMode : presentModes) {
                if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                    // Found a suitable present mode
                }
            }
        }
};


Fl_Vk_Window_Driver *Fl_Vk_Window_Driver::newVkWindowDriver(Fl_Vk_Window *w)
{
  return new Fl_WinAPI_Vk_Window_Driver(w);
}


Fl_Vk_Choice *Fl_WinAPI_Vk_Window_Driver::find(int m, const int *alistp)
{
    Fl_WinAPI_Vk_Choice *g = (Fl_WinAPI_Vk_Choice*)Fl_Vk_Window_Driver::find_begin(m, alistp);
    if (g) return g;

    // Replacement for ChoosePixelFormat() that finds one with an overlay if possible:
    HDC gc = (HDC)(fl_graphics_driver ? fl_graphics_driver->gc() : 0);
    if (!gc) gc = fl_GetDC(0);


    g = new Fl_WinAPI_Vk_Choice(m, alistp, first);
    first = g;

    return g;
}


float Fl_WinAPI_Vk_Window_Driver::pixels_per_unit()
{
    int ns = Fl_Window_Driver::driver(pWindow)->screen_num();
    return Fl::screen_driver()->scale(ns);
}


int Fl_WinAPI_Vk_Window_Driver::mode_(int m, const int *a) {
    int oldmode = mode();
    mode( m); alist(a);
    if (pWindow->shown()) {
        g( find(m, a) );
        if (!g() || (oldmode^m)&(FL_DOUBLE|FL_STEREO)) {
            pWindow->hide();
            pWindow->show();
        }
    } else {
        g(0);
    }
    return 1;
}

void Fl_WinAPI_Vk_Window_Driver::make_current_after() {
}


static signed char swap_interval_type = -1;

static void init_swap_interval() {
  if (swap_interval_type != -1)
    return;
}

void Fl_WinAPI_Vk_Window_Driver::swap_interval(int interval) {
}

int Fl_WinAPI_Vk_Window_Driver::swap_interval() const {
    return -1;
}


int Fl_WinAPI_Vk_Window_Driver::flush_begin(char& valid_f_) {
  return 0;
}



#endif // HAVE_VK
