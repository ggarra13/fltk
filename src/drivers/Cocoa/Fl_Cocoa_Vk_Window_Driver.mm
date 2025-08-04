//
// Class Fl_Cocoa_Vk_Window_Driver for the Fast Light Tool Kit (FLTK).
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

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h> // For vkCreateMetalSurfaceEXT
#include <AppKit/AppKit.h>       // For NSView
#include <Metal/Metal.h>         // For CAMetalLayer
#include <QuartzCore/QuartzCore.h> // For CAMetalLayer

#include <vulkan/vulkan.h>
#include "../../Fl_Vk_Choice.H"
#include "../../Fl_Screen_Driver.H"
#include "Fl_Cocoa_Window_Driver.H"
#include "Fl_Cocoa_Vk_Window_Driver.H"
#include <FL/Fl_Graphics_Driver.H>
#include <FL/Fl_Image_Surface.H>
#include <dlfcn.h>

#import <Cocoa/Cocoa.h>

// Describes crap needed to create a GLContext.
class Fl_Cocoa_Vk_Choice : public Fl_Vk_Choice {
  friend class Fl_Cocoa_Vk_Window_Driver;
private:
  void* pixelformat;
public:
  Fl_Cocoa_Vk_Choice(int m, const int *alistp, Fl_Vk_Choice *n) : Fl_Vk_Choice(m, alistp, n) {
    pixelformat = NULL;
  }
};


Fl_Vk_Window_Driver *Fl_Vk_Window_Driver::newVkWindowDriver(Fl_Vk_Window *w)
{
  return new Fl_Cocoa_Vk_Window_Driver(w);
}


static void* mode_to_NSVulkanPixelFormat(int m, const int *alistp)
{
  return nullptr;
}


Fl_Vk_Choice *Fl_Cocoa_Vk_Window_Driver::find(int m, const int *alistp)
{
  Fl::screen_driver()->open_display(); // useful when called through gl_start()
  Fl_Cocoa_Vk_Choice *g = (Fl_Cocoa_Vk_Choice*)Fl_Vk_Window_Driver::find_begin(m, alistp);
  if (g) return g;
  void* fmt = mode_to_NSVulkanPixelFormat(m, alistp);
  if (!fmt) return 0;
  g = new Fl_Cocoa_Vk_Choice(m, alistp, first);
  first = g;
  g->pixelformat = fmt;
  return g;
}


#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12
#  define NSVulkanContextParameterSurfaceOpacity NSVulkanCPSurfaceOpacity
#endif

float Fl_Cocoa_Vk_Window_Driver::pixels_per_unit()
{
  int retina = (fl_mac_os_version >= 100700 && Fl::use_high_res_GL() && Fl_X::flx(pWindow) &&
          Fl_Cocoa_Window_Driver::driver(pWindow)->mapped_to_retina()) ? 2 : 1;
  return retina * Fl_Graphics_Driver::default_driver().scale();
}

int Fl_Cocoa_Vk_Window_Driver::mode_(int m, const int *a) {
  if (a) { // when the mode is set using the a array of system-dependent values, and if asking for double buffer,
    // the FL_DOUBLE flag must be set in the mode_ member variable
    const int *aa = a;
    while (*aa) {
      if (*(aa++) ==
          kCGLPFADoubleBuffer
          ) { m |= FL_DOUBLE; break; }
    }
  }
  mode( m); alist(a);
  if (pWindow->shown()) {
    g( find(m, a) );
    pWindow->redraw();
  } else {
    g(0);
  }
  return 1;
}


void Fl_Cocoa_Vk_Window_Driver::swap_interval(int n) {
  GLint interval = (GLint)n;
}

int Fl_Cocoa_Vk_Window_Driver::swap_interval() const {
  GLint interval = (GLint)-1;
  return interval;
}



// convert BGRA to RGB and also exchange top and bottom
static uchar *convert_BGRA_to_RGB(uchar *baseAddress, int w, int h, int mByteWidth)
{
  uchar *newimg = new uchar[3*w*h];
  uchar *to = newimg;
  for (int i = h-1; i >= 0; i--) {
    uchar *from = baseAddress + i * mByteWidth;
    for (int j = 0; j < w; j++, from += 4) {
#if defined(__ppc__) && __ppc__
      memcpy(to, from + 1, 3);
      to += 3;
#else
      *(to++) = *(from+2);
      *(to++) = *(from+1);
      *(to++) = *from;
#endif
    }
  }
  delete[] baseAddress;
  return newimg;
}


static Fl_RGB_Image *cgimage_to_rgb4(CGImageRef img) {
  int w = (int)CGImageGetWidth(img);
  int h = (int)CGImageGetHeight(img);
  CGColorSpaceRef cspace = CGColorSpaceCreateDeviceRGB();
  uchar *rgba = new uchar[4 * w * h];
  CGContextRef auxgc = CGBitmapContextCreate(rgba, w, h, 8, 4 * w, cspace,
                                             kCGImageAlphaPremultipliedLast);
  CGColorSpaceRelease(cspace);
  CGContextDrawImage(auxgc, CGRectMake(0, 0, w, h), img);
  CGContextRelease(auxgc);
  Fl_RGB_Image *rgb = new Fl_RGB_Image(rgba, w, h, 4);
  rgb->alloc_array = 1;
  return rgb;
}


// Fl_RGB_Image* Fl_Cocoa_Vk_Window_Driver::capture_vk_rectangle(int x, int y, int w, int h)
// {
//   Fl_Vk_Window* glw = pWindow;
//   float factor = glw->pixels_per_unit();
//   if (factor != 1) {
//     w *= factor; h *= factor; x *= factor; y *= factor;
//   }
// #if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5
//   if (fl_mac_os_version >= 100500) {
//     NSWindow *nswin = (NSWindow*)fl_mac_xid(pWindow);
//     CGImageRef img_full = Fl_Cocoa_Window_Driver::capture_decorated_window_10_5(nswin);
//     int bt =  [nswin frame].size.height - [[nswin contentView] frame].size.height;
//     bt *= (factor / Fl_Graphics_Driver::default_driver().scale());
//     CGRect cgr = CGRectMake(x, y + bt, w, h); // add vertical offset to bypass titlebar
//     CGImageRef cgimg = CGImageCreateWithImageInRect(img_full, cgr); // 10.4
//     CGImageRelease(img_full);
//     Fl_RGB_Image *rgb = cgimage_to_rgb4(cgimg);
//     CGImageRelease(cgimg);
//     return rgb;
//   }
// #endif
//   [(NSVulkanContext*)glw->context() makeCurrentContext];
// // to capture also the overlay and for directGL demo
//   [(NSVulkanContext*)glw->context() flushBuffer];
//   // Read Vulkan context pixels directly.
//   // For extra safety, save & restore Vulkan states that are changed
//   glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
//   glPixelStorei(GL_PACK_ALIGNMENT, 4); /* Force 4-byte alignment */
//   glPixelStorei(GL_PACK_ROW_LENGTH, 0);
//   glPixelStorei(GL_PACK_SKIP_ROWS, 0);
//   glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
//   // Read a block of pixels from the frame buffer
//   int mByteWidth = w * 4;
//   mByteWidth = (mByteWidth + 3) & ~3;    // Align to 4 bytes
//   uchar *baseAddress = new uchar[mByteWidth * h];
//   glReadPixels(x, glw->pixel_h() - (y+h), w, h,
//                GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, baseAddress);
//   glPopClientAttrib();
//   baseAddress = convert_BGRA_to_RGB(baseAddress, w, h, mByteWidth);
//   Fl_RGB_Image *img = new Fl_RGB_Image(baseAddress, w, h, 3, 3 * w);
//   img->alloc_array = 1;
//   return img;
// }

std::vector<const char*> Fl_Cocoa_Vk_Window_Driver::get_instance_extensions()
{
    std::vector<const char*> out;
    out.push_back("VK_KHR_surface");
    out.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
    return out;
}

void Fl_Cocoa_Vk_Window_Driver::create_surface()
{
    FLWindow* window = fl_xid(pWindow);
    
    // Get the NSView from the window                   
    NSView* view = [window contentView];
    
    // Ensure the view has a CAMetalLayer (required by MoltenVK)
    CAMetalLayer* metalLayer = [CAMetalLayer layer];
    [view setLayer:metalLayer];
    [view setWantsLayer:YES]; // Enable layer-backing for the NSView
    
    // Get the scale factor from the window or screen
    CGFloat scale = [[view window] backingScaleFactor];

    // Apply the scale to the layer
    if (Fl::use_high_res_VK())
    {
        metalLayer.contentsScale = scale;
        metalLayer.drawableSize = CGSizeMake(view.bounds.size.width * scale,
                                             view.bounds.size.height * scale);
    }
    
    VkMetalSurfaceCreateInfoEXT surfaceInfo = {};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    surfaceInfo.pNext = nullptr;
    surfaceInfo.flags = 0;
    surfaceInfo.pLayer = (__bridge CAMetalLayer*)metalLayer; // Use the layer instead of the view

    // Use vkCreateMetalSurfaceEXT instead of vkCreateMacOSSurfaceMVK
    VkResult result = vkCreateMetalSurfaceEXT(pWindow->ctx.instance,
                                              &surfaceInfo,
                                              nullptr,
                                              &pWindow->m_surface);
    if (result != VK_SUCCESS)
    {
        Fl::fatal("Failed to create macOS Vulkan surface");
    }
}


void* Fl_Cocoa_Vk_Window_Driver::GetProcAddress(const char *procName) {
  return dlsym(RTLD_DEFAULT, procName);
}


#endif // HAVE_VK
