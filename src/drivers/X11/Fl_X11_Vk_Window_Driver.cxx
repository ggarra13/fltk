//
// Class Fl_X11_Vk_Window_Driver for the Fast Light Tool Kit (FLTK).
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
#include "Fl_X11_Vk_Window_Driver.H"
#include "../../Fl_Vk_Choice.H"
#include "Fl_X11_Window_Driver.H"


// Describes crap needed to create a VKContext.
class Fl_X11_Vk_Choice : public Fl_Vk_Choice {
  friend class Fl_X11_Vk_Window_Driver;
private:
  int pixelformat;           // the visual to use
public:
  Fl_X11_Vk_Choice(int m, const int *alistp, Fl_Vk_Choice *n) : Fl_Vk_Choice(m, alistp, n) {
    pixelformat = 0;
  }
};


Fl_Vk_Window_Driver *Fl_Vk_Window_Driver::newVkWindowDriver(Fl_Vk_Window *w)
{
  return new Fl_X11_Vk_Window_Driver(w);
}


Fl_Vk_Choice *Fl_X11_Vk_Window_Driver::find(int m, const int *alistp)
{
  Fl_X11_Vk_Choice *g = (Fl_X11_Vk_Choice*)Fl_Vk_Window_Driver::find_begin(m, alistp);
  if (g) return g;
  
  g = new Fl_X11_Vk_Choice(m, alistp, first);
  first = g;

  // \@todo: choose visual
  // g->pixelformat = pixelformat;

  return g;
}


float Fl_X11_Vk_Window_Driver::pixels_per_unit()
{
    int ns = Fl_Window_Driver::driver(pWindow)->screen_num();
    return Fl::screen_driver()->scale(ns);
}


int Fl_X11_Vk_Window_Driver::mode_(int m, const int *a) {
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

void* Fl_X11_Vk_Window_Driver::GetProcAddress(const char* name)
{
    //! \@todo: implement dlsym
    return nullptr;
}

void Fl_X11_Vk_Window_Driver::create_surface()
{   
    VkXlibSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    createInfo.dpy    = fl_x11_display();
    createInfo.window = fl_x11_xid(pWindow);
    
    if (vkCreateXlibSurfaceKHR(pWindow->m_instance, &createInfo, nullptr,
                               &pWindow->m_surface) != VK_SUCCESS) {
        Fl::fatal("Failed to create Xlib surface!");
    }
}

void Fl_X11_Vk_Window_Driver::make_current_after() {
}


static signed char swap_interval_type = -1;

static void init_swap_interval() {
  if (swap_interval_type != -1)
    return;
}

std::vector<std::string> Fl_X11_Vk_Window_Driver::get_required_extensions()
{
    std::vector<std::string> out;
    out.push_back("VK_KHR_surface");
    out.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
    return out;
}

void Fl_X11_Vk_Window_Driver::swap_interval(int interval) {
}

int Fl_X11_Vk_Window_Driver::swap_interval() const {
    return -1;
}


int Fl_X11_Vk_Window_Driver::flush_begin(char& valid_f_) {
  return 0;
}

Fl_X11_Vk_Window_Driver::Fl_X11_Vk_Window_Driver(Fl_Vk_Window *win) :
    Fl_Vk_Window_Driver(win)
{
}

Fl_X11_Vk_Window_Driver::~Fl_X11_Vk_Window_Driver()
{
}


#endif // HAVE_VK
