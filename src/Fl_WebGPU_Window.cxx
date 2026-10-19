//
// WebGPU window code for the Fast Light Tool Kit (FLTK).
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

#if HAVE_WEBGPU

#include <webgpu/webgpu.h>

#include <FL/Fl_WebGPU_Window.H>
#include "Fl_WebGPU_Window_Driver.H"
#include "Fl_Window_Driver.H"
#include "Fl_Scalable_Graphics_Driver.H" // Fl_Fontdesc
#include <FL/Fl_Graphics_Driver.H>
#include <FL/fl_utf8.h>
#include <FL/platform.H>

#include <algorithm>
#include <fstream>
#include <iostream>


void Fl_WebGPU_Window::destroy_common_resources() {
}

void Fl_WebGPU_Window::begin_render_pass()
{
  // Color attachment
  WGPURenderPassColorAttachment color_attachment{
    .view = wgpu.back_buffer,
    .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
    .loadOp = WGPULoadOp_Clear,
    .storeOp = WGPUStoreOp_Store,
    .clearValue = clearValues,
  };

  // Render pass descriptor
  char buf[128];
  snprintf(buf, 128, "Main Render Pass for '%s'",
           label() ? label() : "?");

  WGPURenderPassDescriptor render_pass_desc{
      .label = buf,
      .colorAttachmentCount = 1,
      .colorAttachments = &color_attachment,
  };

  // Begin render pass
  wgpu.render_pass = wgpuCommandEncoderBeginRenderPass(wgpu.encoder,
                                                       &render_pass_desc);
  m_in_render_pass = true;
}


void Fl_WebGPU_Window::end_render_pass()
{
  if (m_in_render_pass)
  {
    wgpuRenderPassEncoderEnd(wgpu.render_pass);
    wgpuRenderPassEncoderRelease(wgpu.render_pass);
    m_in_render_pass = false;
  }
}

void Fl_WebGPU_Window::draw_overlay()
{
}

void Fl_WebGPU_Window::draw_begin()
{
    begin_render_pass();
}

/**
   You must implement this virtual function if you want to draw into the
   overlay.  The overlay is cleared before this is called.  You should
   draw anything that is not clear using WebGPU.  You must use
   vk_color(i) to choose colors (it allocates them from the colormap
   using system-specific calls), and remember that you are in an indexed
   WebGPU mode and drawing anything other than flat-shaded will probably
   not work.

   Both this function and Fl_WebGPU_Window::draw() should check
   Fl_WebGPU_Window::valid() and set the same transformation.  If you
   don't your code may not work on other systems.  Depending on the OS,
   and on whether overlays are real or simulated, the WebGPU context may
   be the same or different between the overlay and main window.
*/

/**
   Supports drawing to an Fl_WebGPU_Window with the FLTK 2D drawing API.
   \see \ref vulkan_with_fltk_widgets
*/

/**
   To be used as a match for a previous call to Fl_WebGPU_Window::vk_draw_begin().
   \see \ref vulkan_with_fltk_widgets
*/
void Fl_WebGPU_Window::draw_end()
{
  end_render_pass();

  // End all render passes before finishing.
  WGPUCommandBuffer commands = wgpuCommandEncoderFinish(wgpu.encoder, NULL);
}

/**  Returns non-zero if the hardware supports the given or current WebGPU  mode. */
int Fl_WebGPU_Window::can_do(int a, const int *b) {
  return Fl_WebGPU_Window_Driver::global()->find(a, b) != 0;
}

void Fl_WebGPU_Window::show() {
  int need_after = 0;
  if (!pWebGPUWindowDriver)
    pWebGPUWindowDriver = create_driver();
  if (!shown()) {
    Fl_Window::default_size_range();
    pWebGPUWindowDriver->before_show(need_after);
  }
  Fl_Window::show();
  if (need_after)
    pWebGPUWindowDriver->after_show();

}

int Fl_WebGPU_Window::mode(int m, const int *a) {
  if (m == mode_ && a == alist)
    return 0;
  if (!pWebGPUWindowDriver)
  {
      mode_ = m;
      return m;
  }
  return pWebGPUWindowDriver->mode_(m, a);
}

void Fl_WebGPU_Window::swap_buffers() {

  // End all render passes before finishing.
  WGPUCommandBuffer commands =
    wgpuCommandEncoderFinish(wgpu.encoder, NULL);

  {
    std::lock_guard<std::mutex> lock(queue_mutex());

    wgpuQueueSubmit(wgpu.queue(), 1, &commands);
  }
}

/**
   Sets the rate at which the VK windows swaps buffers.
   This method can be called after the WebGPU context was created, typically
   within the user overridden `Fl_WebGPU_Window::draw()` method that will be
   overridden by the user.
   \note This method depends highly on the underlying WebGPU contexts and driver
   implementation. Most driver seem to accept only 0 and 1 to swap buffer
   asynchronously or in sync with the vertical blank.
   \param[in] frames set the number of vertical frame blanks between WebGPU
   buffer swaps
*/
void Fl_WebGPU_Window::swap_interval(int value) {
  if (pWebGPUWindowDriver->swap_interval() != value)
  {
    pWebGPUWindowDriver->swap_interval(value);
  }
}

/**
   Gets the rate at which the VK windows swaps buffers.
   This method can be called after the WebGPU context was created, typically
   within the user overridden `Fl_WebGPU_Window::draw()` method that will be
   overridden by the user.
   \note This method depends highly on the underlying WebGPU contexts and driver
   implementation. Some drivers return no information, most drivers don't
   support intervals with multiple frames and return only 0 or 1.
   \note Some drivers have the ability to set the swap interval but no way
   to query it, hence this method may return -1 even though the interval was
   set correctly. Conversely a return value greater zero does not guarantee
   that the driver actually honors the setting.
   \return an integer greater zero if vertical blanking is taken into account
   when swapping WebGPU buffers
   \return 0 if the vertical blanking is ignored
   \return -1 if the information can not be retrieved
*/
int Fl_WebGPU_Window::swap_interval() const {
  return pWebGPUWindowDriver->swap_interval();
}


void Fl_WebGPU_Window::flush() {
  if (!shown() || pixel_w() <= 0 || pixel_h() <= 0)
    return;

  draw_begin();
  draw();  // User defined virtual draw function
  draw_end();

  // Submit and present
  swap_buffers();
}


void Fl_WebGPU_Window::resize(int X, int Y, int W, int H) {
  // printf("%s Fl_WebGPU_Window::resize(X=%d, Y=%d, W=%d, H=%d)\n",
  //        vulkan_window_label(this), X, Y, W, H);
  // printf("%s orig: x()=%d, y()=%d, w()=%d, h()=%d pixel_w()=%d pixel_h()=%d\n",
  //        vulkan_window_label(this), x(), y(), w(), h(), pixel_w(), pixel_h());
  int is_a_resize = (W != Fl_Widget::w() || H != Fl_Widget::h() ||
                     is_a_rescale());

  Fl_Window::resize(X, Y, W, H);

  if (!pWebGPUWindowDriver) pWebGPUWindowDriver = create_driver();
  pWebGPUWindowDriver->resize(is_a_resize, W, H);
}

/**
   Hides the window and destroys the WebGPU context.
*/
void Fl_WebGPU_Window::hide() {
  shutdown_webgpu();
  Fl_Window::hide();
}


/** Draws the Fl_WebGPU_Window.
    You \e \b must subclass Fl_WebGPU_Window and provide an implementation for
    draw().

    The draw() method can <I>only</I> use WebGPU calls.  Do not
    attempt to call X, any of the functions in <FL/fl_draw.H>, WebGPU or vk
    directly.  Do not call vk_start() or vk_finish().

    Double-buffering is enabled by default in the window, the back and front
    buffers are swapped after this function is completed.

    \code
    void mywindow::draw() {
    ... draw your geometry here ...
    }
    \endcode

    Actual example code to clear screen to blue:
    \code
    void mywindow::draw() {
    Fl_WebGPU_Window::draw();
    }
    \endcode

    Regular FLTK widgets can be added as children to the Fl_WebGPU_Window. To
    correctly overlay the widgets, Fl_WebGPU_Window::draw() must be called after
    rendering the main scene.
    \code

    void mywindow::vk_draw_begin()
    {
    // Background color
    m_clearColor = { 0.0, 0.0, 0.0, 0.0 };

    // Clear the screen and prepare m_draw_cmd
    Fl_WebGPU_Window::vk_draw_begin();
    }

    void mywindow::draw() {
    // draw 3d graphics scene
    Fl_WebGPU_Window::draw();
    // -- or --
    Fl_Window::draw();
    // other 2d drawing calls, overlays, etc.
    }
    \endcode

*/
void Fl_WebGPU_Window::draw() {
  Fl_Window::draw();
}
/**
   Handle some FLTK events as needed.
*/
int Fl_WebGPU_Window::handle(int event) {
  return Fl_Window::handle(event);
}

/** The number of pixels per FLTK unit of length for the window.
    This method dynamically adjusts its value when the GUI is rescaled or when the window
    is moved to/from displays of distinct resolutions. This method is useful, e.g., to convert,
    in a window's handle() method, the FLTK units returned by Fl::event_x() and
    Fl::event_y() to the pixel units used by the WebGPU source code.
    \version 1.3.4
*/
float Fl_WebGPU_Window::pixels_per_unit() {
  if (!pWebGPUWindowDriver) return 1.0;
  return pWebGPUWindowDriver->pixels_per_unit();
}


/**
   \cond DriverDev
   \addtogroup DriverDeveloper
   \{
*/

Fl_Window *Fl_WebGPU_Window_Driver::cached_window = NULL;
// scaling factor between FLTK and VK drawing units: VK = FLTK * vk_scale
float Fl_WebGPU_Window_Driver::vk_scale = 1;

void Fl_WebGPU_Window_Driver::invalidate() {}

void Fl_WebGPU_Window::shutdown_webgpu() {
  // Destroy swapchain and surface
  pWebGPUWindowDriver->destroy_resources();
  pWebGPUWindowDriver->destroy_surface();

  //webgpu.destroy();
}


/**
   The destructor removes the widget and destroys the WebGPU context
   associated with it.
*/
Fl_WebGPU_Window::~Fl_WebGPU_Window()
{
  shutdown_webgpu();
  delete pWebGPUWindowDriver;
  pWebGPUWindowDriver = nullptr;
}

void Fl_WebGPU_Window::init_colorspace()
{
  pWebGPUWindowDriver->init_colorspace();
}

void Fl_WebGPU_Window::init_webgpu() {

  // Destroy surface
  if (wgpu.surface)
    pWebGPUWindowDriver->destroy_surface();

  pWebGPUWindowDriver->create_surface();

  // \@todo: Initialize colorspace
  // init_colorspace();

  // \@todo: WebGPU does not support vkSetHdrMetadata equivalent yet
  //   if (!vkSetHdrMetadataEXT)
  //   {
  //     bool found_hdr = false;
  //     for (auto extension : ctx.device_extensions)
  //     {
  //       if (strcmp(extension, VK_EXT_HDR_METADATA_EXTENSION_NAME) == 0)
  //       {
  //         found_hdr = true;
  //       }
  //     }

  //     if (found_hdr)
  //     {
  //       vkSetHdrMetadataEXT = (PFN_vkSetHdrMetadataEXT)vkGetDeviceProcAddr(device(), "vkSetHdrMetadataEXT");
  //     }
  //   }

  // Verify window size
  if (w() <= 0 || h() <= 0) {
    pWebGPUWindowDriver->destroy_surface();
    return;
  }

  // Initialize swapchain views, depth/stencil and framebuffers
  pWebGPUWindowDriver->prepare();
}


Fl_WebGPU_Window_Driver *Fl_WebGPU_Window::create_driver() {
  return Fl_WebGPU_Window_Driver::newWebGPUWindowDriver(this);
}

void Fl_WebGPU_Window::init() {
  end(); // we probably don't want any children
  box(FL_NO_BOX);

  pWebGPUWindowDriver = nullptr;

  mode_ = FL_RGB | FL_DEPTH | FL_DOUBLE;
  alist = 0;
  g = 0;
  damage1_ = 0;

  // Reset WebGPU Handles
#ifdef NDEBUG
  m_debugSync = false;
#else
  m_debugSync = false;
#endif
}

/**
   \}
   \endcond
*/

#endif // HAVE_VK
