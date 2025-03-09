//
// Vulkan window code for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2022 by Bill Spitzak and others.
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
#if HAVE_VULKAN

extern int fl_vk_load_plugin;

// #include <FL/vk.h>  // should it be FL/gl.h?
#include <FL/Fl_Vk_Window.H>  // \@done
#include "Fl_Vk_Window_Driver.H"
#include "Fl_Window_Driver.H"  // \@done
#include <FL/Fl_Graphics_Driver.H> // \@done
#include <FL/fl_utf8.h>            // \@done
#include "drivers/Vulkan/Fl_Vulkan_Display_Device.H"
#include "drivers/Vulkan/Fl_Vulkan_Graphics_Driver.H"

#include <stdlib.h>
#  if (HAVE_DLSYM && HAVE_DLFCN_H)
#    include <dlfcn.h>
#  endif // (HAVE_DLSYM && HAVE_DLFCN_H)


////////////////////////////////////////////////////////////////

// The symbol SWAP_TYPE defines what is in the back buffer after doing
// a vkXSwapBuffers().

// The OpenVk documentation says that the contents of the backbuffer
// are "undefined" after vkXSwapBuffers().  However, if we know what
// is in the backbuffers then we can save a good deal of time.  For
// this reason you can define some symbols to describe what is left in
// the back buffer.

// Having not found any way to determine this from vkx (or wvk) I have
// resorted to letting the user specify it with an environment variable,
// VK_SWAP_TYPE, it should be equal to one of these symbols:

// contents of back buffer after vkXSwapBuffers():
#define UNDEFINED 1     // anything
#define SWAP 2          // former front buffer (same as unknown)
#define COPY 3          // unchanged
#define NODAMAGE 4      // unchanged even by X expose() events

static char SWAP_TYPE = 0 ; // 0 = determine it from environment variable


/**  Returns non-zero if the hardware supports the given or current Vulkan  mode. */
int Fl_Vk_Window::can_do(int a, const int *b) {
  return Fl_Vk_Window_Driver::vkobal()->find(a,b) != 0;
}

void Fl_Vk_Window::show() {
  int need_after = 0;
  if (!shown()) {
    Fl_Window::default_size_range();
    if (!g) {
      g = pVkWindowDriver->find(mode_,alist);
      if (!g && (mode_ & FL_DOUBLE) == FL_SINVKE) {
        g = pVkWindowDriver->find(mode_ | FL_DOUBLE,alist);
        if (g) mode_ |= FL_FAKE_SINVKE;
      }

      if (!g) {
        Fl::error("Insufficient VK support");
        return;
      }
    }
    pVkWindowDriver->before_show(need_after);
  }
  Fl_Window::show();
  if (need_after) pVkWindowDriver->after_show();
}


/**
  The invalidate() method turns off valid() and is
  equivalent to calling value(0).
*/
void Fl_Vk_Window::invalidate() {
  valid(0);
  context_valid(0);
  pVkWindowDriver->invalidate();
}

int Fl_Vk_Window::mode(int m, const int *a) {
  if (m == mode_ && a == alist) return 0;
  return pVkWindowDriver->mode_(m, a);
}

#define NON_LOCAL_CONTEXT 0x80000000

/**
  The make_current() method selects the Vulkan context for the
  widget.  It is called automatically prior to the draw() method
  being called and can also be used to implement feedback and/or
  selection within the handle() method.
*/

void Fl_Vk_Window::make_current() {
  puts("Fl_Vk_Window::make_current()");
//  printf("make_current: context_=%p\n", context_);
    
  // pVkWindowDriver->make_current_before();
  // if (!context_) {
  //   mode_ &= ~NON_LOCAL_CONTEXT;
  //   context_ = pVkWindowDriver->create_vk_context(this, g);
  //   valid(0);
  //   context_valid(0);
  // }
  // pVkWindowDriver->set_vk_context(this, context_);
  // pVkWindowDriver->make_current_after();
  // if (mode_ & FL_FAKE_SINGLE) {
  //   vkDrawBuffer(VK_FRONT);
  //   vkReadBuffer(VK_FRONT);
  // }

    current_ = this;
}

/**
  Sets the projection so 0,0 is in the lower left of the window and each
  pixel is 1 unit wide/tall.  If you are drawing 2D images, your
  draw() method may want to call this if valid() is false.
*/
void Fl_Vk_Window::ortho()
{
}

/**
  The swap_buffers() method swaps the back and front buffers.
  It is called automatically after the draw() method is called.
*/
void Fl_Vk_Window::swap_buffers() {
  pVkWindowDriver->swap_buffers();
}

/**
 Sets the rate at which the VK windows swaps buffers.
 This method can be called after the Vulkan context was created, typically
 within the user overridden `Fl_Vk_Window::draw()` method that will be
 overridden by the user.
 \note This method depends highly on the underlying Vulkan contexts and driver
    implementation. Most driver seem to accept only 0 and 1 to swap buffer
    asynchronously or in sync with the vertical blank.
 \param[in] frames set the number of vertical frame blanks between Vulkan
    buffer swaps
 */
void Fl_Vk_Window::swap_interval(int frames) {
  pVkWindowDriver->swap_interval(frames);
}

/**
 Gets the rate at which the VK windows swaps buffers.
 This method can be called after the Vulkan context was created, typically
 within the user overridden `Fl_Vk_Window::draw()` method that will be
 overridden by the user.
 \note This method depends highly on the underlying Vulkan contexts and driver
    implementation. Some drivers return no information, most drivers don't
    support intervals with multiple frames and return only 0 or 1.
 \note Some drivers have the ability to set the swap interval but no way
    to query it, hence this method may return -1 even though the interval was
    set correctly. Conversely a return value greater zero does not guarantee
    that the driver actually honors the setting.
 \return an integer greater zero if vertical blanking is taken into account
    when swapping Vulkan buffers
 \return 0 if the vertical blanking is ignored
 \return -1 if the information can not be retrieved
 */
int Fl_Vk_Window::swap_interval() const {
  return pVkWindowDriver->swap_interval();
}


void Fl_Vk_Window::flush() {
  if (!shown()) return;
  uchar save_valid = valid_f_ & 1;
  if (pVkWindowDriver->flush_begin(valid_f_) ) return;
  make_current();

  if (mode_ & FL_DOUBLE) {

    vkDrawBuffer(VK_BACK);

    if (!SWAP_TYPE) {
      SWAP_TYPE = pVkWindowDriver->swap_type();
      const char* c = fl_getenv("VK_SWAP_TYPE");
      if (c) {
        if (!strcmp(c,"COPY")) SWAP_TYPE = COPY;
        else if (!strcmp(c, "NODAMAGE")) SWAP_TYPE = NODAMAGE;
        else if (!strcmp(c, "SWAP")) SWAP_TYPE = SWAP;
        else SWAP_TYPE = UNDEFINED;
      }
    }

    if (SWAP_TYPE == NODAMAGE) {

      // don't draw if only overlay damage or expose events:
      if ((damage()&~(FL_DAMAGE_OVERLAY|FL_DAMAGE_EXPOSE)) || !save_valid)
        draw();
      swap_buffers();

    } else if (SWAP_TYPE == COPY) {

      // don't draw if only the overlay is damaged:
      if (damage() != FL_DAMAGE_OVERLAY || !save_valid) draw();
          swap_buffers();

    } else if (SWAP_TYPE == SWAP){
      damage(FL_DAMAGE_ALL);
      draw();
      if (overlay == this) draw_overlay();
      swap_buffers();
    } else if (SWAP_TYPE == UNDEFINED){ // SWAP_TYPE == UNDEFINED

      // If we are faking the overlay, use CopyPixels to act like
      // SWAP_TYPE == COPY.  Otherwise overlay redraw is way too slow.
      if (overlay == this) {
        // don't draw if only the overlay is damaged:
        if (damage1_ || damage() != FL_DAMAGE_OVERLAY || !save_valid) draw();
        // we use a separate context for the copy because rasterpos must be 0
        // and depth test needs to be off:
        static VKContext ortho_context = 0;
        static Fl_Vk_Window* ortho_window = 0;
        int orthoinit = !ortho_context;
        if (orthoinit) ortho_context = pVkWindowDriver->create_vk_context(this, g);
        pVkWindowDriver->set_vk_context(this, ortho_context);
        if (orthoinit || !save_valid || ortho_window != this) {
          vkDisable(VK_DEPTH_TEST);
          vkReadBuffer(VK_BACK);
          vkDrawBuffer(VK_FRONT);
          vkLoadIdentity();
          vkViewport(0, 0, pixel_w(), pixel_h());
          vkOrtho(0, pixel_w(), 0, pixel_h(), -1, 1);
          vkRasterPos2i(0,0);
          ortho_window = this;
        }
        vkCopyPixels(0,0,pixel_w(),pixel_h(),VK_COLOR);
        make_current(); // set current context back to draw overlay
        damage1_ = 0;

      } else {
        damage1_ = damage();
        clear_damage(0xff); draw();
        swap_buffers();
      }

    }
    if (overlay==this && SWAP_TYPE != SWAP) { // fake overlay in front buffer
      vkDrawBuffer(VK_FRONT);
      draw_overlay();
      vkDrawBuffer(VK_BACK);
      vkFlush();
    }

  } else {      // sinvke-buffered context is simpler:

    draw();
    if (overlay == this) draw_overlay();
    vkFlush();

  }

  valid(1);
  context_valid(1);
}

void Fl_Vk_Window::resize(int X,int Y,int W,int H) {
//  printf("Fl_Vk_Window::resize(X=%d, Y=%d, W=%d, H=%d)\n", X, Y, W, H);
//  printf("current: x()=%d, y()=%d, w()=%d, h()=%d\n", x(), y(), w(), h());

  int is_a_resize = (W != Fl_Widget::w() || H != Fl_Widget::h() || is_a_rescale());
  if (is_a_resize) valid(0);
  pVkWindowDriver->resize(is_a_resize, W, H);
  Fl_Window::resize(X,Y,W,H);
}

/**
  Sets a pointer to the VKContext that this window is using.
  This is a system-dependent structure, but it is portable to copy
  the context from one window to another. You can also set it to NULL,
  which will force FLTK to recreate the context the next time make_current()
  is called, this is useful for getting around bugs in Vulkan implementations.

  If <i>destroy_flag</i> is true the context will be destroyed by
  fltk when the window is destroyed, or when the mode() is changed,
  or the next time context(x) is called.
*/
void Fl_Vk_Window::context(VKContext v, int destroy_flag) {
  if (context_ && !(mode_&NON_LOCAL_CONTEXT)) pVkWindowDriver->delete_vk_context(context_);
  context_ = v;
  if (destroy_flag) mode_ &= ~NON_LOCAL_CONTEXT;
  else mode_ |= NON_LOCAL_CONTEXT;
}

/**
  Hides the window and destroys the Vulkan context.
*/
void Fl_Vk_Window::hide() {
  context(0);
  pVkWindowDriver->vk_hide_before(overlay);
  Fl_Window::hide();
}

/**
  The destructor removes the widget and destroys the Vulkan context
  associated with it.
*/
Fl_Vk_Window::~Fl_Vk_Window() {
  hide();
//  delete overlay; this is done by ~Fl_Group
  delete pVkWindowDriver;
}

void Fl_Vk_Window::init() {
  pVkWindowDriver = Fl_Vk_Window_Driver::newVkWindowDriver(this);
  end(); // we probably don't want any children
  box(FL_NO_BOX);

  mode_    = FL_RGB | FL_DEPTH | FL_DOUBLE;
  alist    = 0;
  context_ = 0;
  g        = 0;
  overlay  = 0;
  valid_f_ = 0;
  damage1_ = 0;

#if 0 // This breaks resizing on Linux/X11
  int H = h();
  h(1); // Make sure we actually do something in resize()...
  resize(x(), y(), w(), H);
#endif // 0
}

/**
  You must implement this virtual function if you want to draw into the
  overlay.  The overlay is cleared before this is called.  You should
  draw anything that is not clear using Vulkan.  You must use
  vk_color(i) to choose colors (it allocates them from the colormap
  using system-specific calls), and remember that you are in an indexed
  Vulkan mode and drawing anything other than flat-shaded will probably
  not work.

  Both this function and Fl_Vk_Window::draw() should check
  Fl_Vk_Window::valid() and set the same transformation.  If you
  don't your code may not work on other systems.  Depending on the OS,
  and on whether overlays are real or simulated, the Vulkan context may
  be the same or different between the overlay and main window.
*/
void Fl_Vk_Window::draw_overlay() {}

/**
 Supports drawing to an Fl_Vk_Window with the FLTK 2D drawing API.
 \see \ref openvk_with_fltk_widgets
 */
void Fl_Vk_Window::draw_begin() {
  if (mode() & FL_OPENVK3) pVkWindowDriver->switch_to_VK1();
  damage(FL_DAMAGE_ALL); // always redraw all VK widgets above the VK scene
  Fl_Surface_Device::push_current( Fl_Vulkan_Display_Device::display_device() );
  Fl_Vulkan_Graphics_Driver *drv = (Fl_Vulkan_Graphics_Driver*)Fl_Surface_Device::surface()->driver();
  drv->pixels_per_unit_ = pixels_per_unit();

  if (!valid()) {
    vkViewport(0, 0, pixel_w(), pixel_h());
    valid(1);
  }

  vkPushAttrib(VK_ALL_ATTRIB_BITS);

  vkMatrixMode(VK_PROJECTION);
  vkPushMatrix();
  vkLoadIdentity();
  vkOrtho(0.0, w(), h(), 0.0, -1.0, 1.0);

  vkMatrixMode(VK_MODELVIEW);
  vkPushMatrix();
  vkLoadIdentity();

  vkDisable(VK_DEPTH_TEST);
  vkDisable(VK_LIGHTING);
  vkDisable(VK_TEXTURE_2D);
  vkEnable(VK_POINT_SMOOTH);

  vkLineWidth((VKfloat)(drv->pixels_per_unit_*drv->line_width_));
  vkPointSize((VKfloat)(drv->pixels_per_unit_));
  vkBlendFunc(VK_SRC_ALPHA, VK_ONE_MINUS_SRC_ALPHA);
  vkEnable(VK_BLEND);
  if (!pVkWindowDriver->need_scissor()) vkDisable(VK_SCISSOR_TEST);
}

/**
 To be used as a match for a previous call to Fl_Vk_Window::draw_begin().
 \see \ref openvk_with_fltk_widgets
 */
void Fl_Vk_Window::draw_end() {
  vkMatrixMode(VK_MODELVIEW);
  vkPopMatrix();

  vkMatrixMode(VK_PROJECTION);
  vkPopMatrix();

  vkPopAttrib(); // VK_ALL_ATTRIB_BITS

  Fl_Surface_Device::pop_current();
  if (mode() & FL_OPENVK3) pVkWindowDriver->switch_back();
}

/** Draws the Fl_Vk_Window.
  You \e \b must subclass Fl_Vk_Window and provide an implementation for
  draw().  You may also provide an implementation of draw_overlay()
  if you want to draw into the overlay planes.  You can avoid
  reinitializing the viewport and lights and other things by checking
  valid() at the start of draw() and only doing the
  initialization if it is false.

  The draw() method can <I>only</I> use Vulkan calls.  Do not
  attempt to call X, any of the functions in <FL/fl_draw.H>, or vkX
  directly.  Do not call vk_start() or vk_finish().

  If double-buffering is enabled in the window, the back and front
  buffers are swapped after this function is completed.

  The following pseudo-code shows how to use "if (!valid())"
  to initialize the viewport:

  \code
    void mywindow::draw() {
     if (!valid()) {
         vkViewport(0,0,pixel_w(),pixel_h());
         vkFrustum(...) or vkOrtho(...)
         ...other initialization...
     }
     if (!context_valid()) {
         ...load textures, etc. ...
     }
     // clear screen
     vkClearColor(...);
     vkClear(...);
     ... draw your geometry here ...
    }
  \endcode

  Actual example code to clear screen to black and draw a 2D white "X":
  \code
    void mywindow::draw() {
        if (!valid()) {
            vkLoadIdentity();
            vkViewport(0,0,pixel_w(),pixel_h());
            vkOrtho(-w(),w(),-h(),h(),-1,1);
        }
        // Clear screen
        vkClear(VK_COLOR_BUFFER_BIT);
        // Draw white 'X'
        vkColor3f(1.0, 1.0, 1.0);
        vkBegin(VK_LINE_STRIP); vkVertex2f(w(), h()); vkVertex2f(-w(),-h()); vkEnd();
        vkBegin(VK_LINE_STRIP); vkVertex2f(w(),-h()); vkVertex2f(-w(), h()); vkEnd();
    }
  \endcode

  Regular FLTK widgets can be added as children to the Fl_Vk_Window. To
  correctly overlay the widgets, Fl_Vk_Window::draw() must be called after
  rendering the main scene.
  \code
  void mywindow::draw() {
    // draw 3d graphics scene
    Fl_Vk_Window::draw();
    // -- or --
    draw_begin();
    Fl_Window::draw();
    // other 2d drawing calls, overlays, etc.
    draw_end();
  }
  \endcode

*/
void Fl_Vk_Window::draw() {
  draw_begin();
  Fl_Window::draw();
  draw_end();
}

/**
 Handle some FLTK events as needed.
 */
int Fl_Vk_Window::handle(int event)
{
  return Fl_Window::handle(event);
}

// don't remove me! this serves only to force linking of Fl_Vk_Device_Plugin.o
int Fl_Vk_Window::vk_plugin_linkage() {
  return fl_vk_load_plugin;
}

/** The number of pixels per FLTK unit of length for the window.
 This method dynamically adjusts its value when the GUI is rescaled or when the window
 is moved to/from displays of distinct resolutions. This method is useful, e.g., to convert,
 in a window's handle() method, the FLTK units returned by Fl::event_x() and
 Fl::event_y() to the pixel units used by the Vulkan source code.
 \version 1.3.4
 */
float Fl_Vk_Window::pixels_per_unit() {
  return pVkWindowDriver->pixels_per_unit();
}

/**
 \cond DriverDev
 \addtogroup DriverDeveloper
 \{
 */

int Fl_Vk_Window_Driver::copy = COPY;
Fl_Window* Fl_Vk_Window_Driver::cached_window = NULL;
float Fl_Vk_Window_Driver::vk_scale = 1; // scaling factor between FLTK and VK drawing units: VK = FLTK * vk_scale

// creates a unique, dummy Fl_Vk_Window_Driver object used when no Fl_Vk_Window is around
// necessary to support vk_start()/vk_finish()
Fl_Vk_Window_Driver *Fl_Vk_Window_Driver::global() {
  static Fl_Vk_Window_Driver *gwd = newVkWindowDriver(NULL);
  return gwd;
}

void Fl_Vk_Window_Driver::invalidate() {
  if (pWindow->overlay) {
    ((Fl_Vk_Window*)pWindow->overlay)->valid(0);
    ((Fl_Vk_Window*)pWindow->overlay)->context_valid(0);
  }
}


char Fl_Vk_Window_Driver::swap_type() {return UNDEFINED;}


void* Fl_Vk_Window_Driver::GetProcAddress(const char *procName) {
  return NULL;
}

Fl_Font_Descriptor** Fl_Vk_Window_Driver::fontnum_to_fontdescriptor(int fnum) {
  extern FL_EXPORT Fl_Fontdesc *fl_fonts;
  return &(fl_fonts[fnum].first);
}

/* Captures a rectanvke of a Fl_Vk_Window and returns it as an RGB image.
 This is the platform-independent version. Some platforms may override it.
 */
Fl_RGB_Image* Fl_Vk_Window_Driver::capture_vk_rectanvke(int x, int y, int w, int h)
{
}

/**
 \}
 \endcond
 */

#endif // HAVE_VK
