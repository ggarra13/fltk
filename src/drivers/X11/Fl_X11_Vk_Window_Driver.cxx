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

#include <stdexcept>
#include <dlfcn.h>

#include <FL/platform.H>
#include "../../Fl_Screen_Driver.H"

#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>

#include "Fl_X11_Vk_Window_Driver.H"
#include "../../Fl_Vk_Choice.H"
#include "Fl_X11_Window_Driver.H"
#if !(USE_XFT || FLTK_USE_CAIRO)
#include "../Xlib/Fl_Font.H"
#endif


// Describes crap needed to create a VKContext.
class Fl_X11_Vk_Choice : public Fl_Vk_Choice {
  friend class Fl_X11_Vk_Window_Driver;

private:
  int pixelformat; // the visual to use
public:
  Fl_X11_Vk_Choice(int m, const int *alistp, Fl_Vk_Choice *n)
    : Fl_Vk_Choice(m, alistp, n) {
    pixelformat = 0;
  }
};

#ifndef FLTK_USE_WAYLAND
Fl_Vk_Window_Driver *Fl_Vk_Window_Driver::newVkWindowDriver(Fl_Vk_Window *w) {
  return new Fl_X11_Vk_Window_Driver(w);
}
#endif

int Fl_X11_Vk_Window_Driver::genlistsize() {
#if USE_XFT || FLTK_USE_CAIRO
  return 256;
#else
  return 0x10000;
#endif
}

void Fl_X11_Vk_Window_Driver::get_list(Fl_Font_Descriptor *fd, int r) {
#if USE_XFT || FLTK_USE_CAIRO
  /* We hope not to come here: We hope that any system using XFT will also
   * have sufficient Vulkan capability to support our font texture pile
   * mechansim, allowing XFT to render the face directly. */
  (void)fd;
  (void)r;
#else
  // @todo:
  // Fl_Xlib_Font_Descriptor *gl_fd = (Fl_Xlib_Font_Descriptor*)fd;
  // if (gl_fd->vkok[r]) return;
  // gl_fd->vkok[r] = 1;
  // unsigned int ii = r * 0x400;
  // for (int i = 0; i < 0x400; i++) {
  //   XFontStruct *font = NULL;
  //   unsigned short id;
  //   fl_XGetUtf8FontAndGlyph(gl_fd->font, ii, &font, &id);
  //   // if (font) glXUseXFont(font->fid, id, 1, gl_fd->listbase+ii);
  //   ii++;
  // }
#endif
}

#if !(USE_XFT || FLTK_USE_CAIRO)
Fl_Font_Descriptor **Fl_X11_Vk_Window_Driver::fontnum_to_fontdescriptor(int fnum) {
  Fl_Xlib_Fontdesc *s = ((Fl_Xlib_Fontdesc *)fl_fonts) + fnum;
  return &(s->first);
}
#endif

Fl_Vk_Choice *Fl_X11_Vk_Window_Driver::find(int m, const int *alistp) {
  Fl_X11_Vk_Choice *g = (Fl_X11_Vk_Choice *)Fl_Vk_Window_Driver::find_begin(m, alistp);
  if (g)
    return g;

  g = new Fl_X11_Vk_Choice(m, alistp, first);
  first = g;

  // \@todo: choose visual
  // g->pixelformat = pixelformat;

  return g;
}


float Fl_X11_Vk_Window_Driver::pixels_per_unit() {
  int ns = Fl_Window_Driver::driver(pWindow)->screen_num();
  return Fl::screen_driver()->scale(ns);
}


int Fl_X11_Vk_Window_Driver::mode_(int m, const int *a) {
  int oldmode = mode();
  mode(m);
  alist(a);
  if (pWindow->shown()) {
    g(find(m, a));
    if (!g() || (oldmode ^ m) & (FL_DOUBLE | FL_STEREO)) {
      pWindow->hide();
      pWindow->show();
    }
  } else {
    g(0);
  }
  return 1;
}

void *Fl_X11_Vk_Window_Driver::GetProcAddress(const char *procName) {
  return dlsym(RTLD_DEFAULT, procName);
}

void Fl_X11_Vk_Window_Driver::create_surface() {
  VkXlibSurfaceCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
  createInfo.dpy = fl_x11_display();
  createInfo.window = fl_x11_xid(pWindow);

  if (vkCreateXlibSurfaceKHR(pWindow->m_instance, &createInfo, nullptr, &pWindow->m_surface) !=
      VK_SUCCESS) {
    Fl::fatal("Failed to create Xlib surface!");
  }
}

void Fl_X11_Vk_Window_Driver::make_current_after() {}


static signed char swap_interval_type = -1;

static void init_swap_interval() {
  if (swap_interval_type != -1)
    return;
}

std::vector<const char*> Fl_X11_Vk_Window_Driver::get_required_extensions() {
  std::vector<const char*> out;
  out.push_back("VK_KHR_surface");
  out.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
  return out;
}

void Fl_X11_Vk_Window_Driver::swap_interval(int interval) {}

int Fl_X11_Vk_Window_Driver::swap_interval() const {
  return -1;
}


int Fl_X11_Vk_Window_Driver::flush_begin() {
  return 0;
}

Fl_X11_Vk_Window_Driver::~Fl_X11_Vk_Window_Driver() {}


#endif // HAVE_VK
