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
#include <iostream>
#include <stdexcept>

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
  int pixelformat;           // the visual to use
  PIXELFORMATDESCRIPTOR pfd; // some wgl calls need this thing
public:
  Fl_WinAPI_Vk_Choice(int m, const int *alistp, Fl_Vk_Choice *n) : Fl_Vk_Choice(m, alistp, n) {
    pixelformat = 0;
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

  int pixelformat = 0;
  PIXELFORMATDESCRIPTOR chosen_pfd = {};

  /*
  for (int i = 1; ; i++) {
    PIXELFORMATDESCRIPTOR pfd;
    if (!DescribePixelFormat(gc, i, sizeof(pfd), &pfd)) break;

    if (~pfd.dwFlags & (PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL)) continue;
    if (pfd.iPixelType != ((m&FL_INDEX)?PFD_TYPE_COLORINDEX:PFD_TYPE_RGBA)) continue;
    if ((m & FL_ALPHA) && !pfd.cAlphaBits) continue;
    if ((m & FL_ACCUM) && !pfd.cAccumBits) continue;
    if ((!(m & FL_DOUBLE)) != (!(pfd.dwFlags & PFD_DOUBLEBUFFER))) continue;
    if ((!(m & FL_STEREO)) != (!(pfd.dwFlags & PFD_STEREO))) continue;
    if ((m & FL_DEPTH) && !pfd.cDepthBits) continue;
    if ((m & FL_STENCIL) && !pfd.cStencilBits) continue;

#if DEBUG_PFD
    printf("pfd #%d supports composition: %s\n", i, (pfd.dwFlags & PFD_SUPPORT_COMPOSITION) ? "yes" : "no");
    printf("    ... & PFD_GENERIC_FORMAT: %s\n", (pfd.dwFlags & PFD_GENERIC_FORMAT) ? "generic" : "accelerated");
    printf("    ... Overlay Planes      : %d\n", pfd.bReserved & 15);
    printf("    ... Color & Depth       : %d, %d\n", pfd.cColorBits, pfd.cDepthBits);
    if (pixelformat)
      printf("        current pixelformat : %d\n", pixelformat);
    fflush(stdout);
#endif // DEBUG_PFD

    // see if better than the one we have already:
    if (pixelformat) {
      // offering non-generic rendering is better (read: hardware acceleration)
      if (!(chosen_pfd.dwFlags & PFD_GENERIC_FORMAT) &&
          (pfd.dwFlags & PFD_GENERIC_FORMAT)) continue;
      // offering overlay is better:
      else if (!(chosen_pfd.bReserved & 15) && (pfd.bReserved & 15)) {}
      // otherwise prefer a format that supports composition (STR #3119)
      else if ((chosen_pfd.dwFlags & PFD_SUPPORT_COMPOSITION) &&
               !(pfd.dwFlags & PFD_SUPPORT_COMPOSITION)) continue;
      // otherwise more bit planes is better, but no more than 32 (8 bits per channel):
      else if (pfd.cColorBits > 32 || chosen_pfd.cColorBits > pfd.cColorBits) continue;
      else if (chosen_pfd.cDepthBits > pfd.cDepthBits) continue;
    }
    pixelformat = i;
    chosen_pfd = pfd;
  }

#if DEBUG_PFD
  static int bb = 0;
  if (!bb) {
    bb = 1;
    printf("PFD_SUPPORT_COMPOSITION = 0x%x\n", PFD_SUPPORT_COMPOSITION);
  }
  printf("Chosen pixel format is %d\n", pixelformat);
  printf("Color bits = %d, Depth bits = %d\n", chosen_pfd.cColorBits, chosen_pfd.cDepthBits);
  printf("Pixel format supports composition: %s\n", (chosen_pfd.dwFlags & PFD_SUPPORT_COMPOSITION) ? "yes" : "no");
  fflush(stdout);
#endif // DEBUG_PFD

  if (!pixelformat) return 0;
  */
  
  g = new Fl_WinAPI_Vk_Choice(m, alistp, first);
  first = g;

  g->pixelformat = pixelformat;
  g->pfd = chosen_pfd;

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

void* Fl_WinAPI_Vk_Window_Driver::GetProcAddress(const char* name)
{
    return (void*) ::GetProcAddress(module, name);
}

void Fl_WinAPI_Vk_Window_Driver::create_surface()
{
    VkWin32SurfaceCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = (HWND)fl_win32_xid(pWindow); // Get the HWND from FLTK
    createInfo.hinstance = GetModuleHandle(nullptr); // Get the instance handle


    
    if (vkCreateWin32SurfaceKHR(pWindow->m_instance, &createInfo, nullptr,
                                &pWindow->m_surface) != VK_SUCCESS) {
        Fl::fatal("Win32 Vulkan: failed to create window surface!");
    }
}

void Fl_WinAPI_Vk_Window_Driver::make_current_after() {
}


static signed char swap_interval_type = -1;

static void init_swap_interval() {
  if (swap_interval_type != -1)
    return;
}

std::vector<std::string> Fl_WinAPI_Vk_Window_Driver::get_required_extensions()
{
    std::vector<std::string> out;
    out.push_back("VK_KHR_surface");
    out.push_back("VK_KHR_win32_surface");
    return out;
}

void Fl_WinAPI_Vk_Window_Driver::swap_interval(int interval) {
}

int Fl_WinAPI_Vk_Window_Driver::swap_interval() const {
    return -1;
}


int Fl_WinAPI_Vk_Window_Driver::flush_begin() {
  return 0;
}

Fl_WinAPI_Vk_Window_Driver::Fl_WinAPI_Vk_Window_Driver(Fl_Vk_Window *win) :
    Fl_Vk_Window_Driver(win)
{
    module = LoadLibraryA("vulkan-1.dll");
    if (!module)
    {
        Fl::fatal("Failed to load vulkan-1.dll\n\n"
                  "Do you have Vulkan installed?");
    }
}

Fl_WinAPI_Vk_Window_Driver::~Fl_WinAPI_Vk_Window_Driver()
{
    FreeLibrary(module);
}


#endif // HAVE_VK
