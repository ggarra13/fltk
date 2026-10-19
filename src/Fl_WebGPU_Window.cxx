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

#include <FL/Fl.H>
#include <FL/Fl_WebGPU_Window.H>
#include "Fl_WebGPU_Window_Driver.H"
#include "Fl_Window_Driver.H"
#include "Fl_Scalable_Graphics_Driver.H" // Fl_Fontdesc
#include <FL/Fl_Graphics_Driver.H>
#include <FL/fl_utf8.h>
#include <FL/platform.H>

#include <cstdio>


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

  // Depth/stencil attachment, if the window mode asked for one
  WGPURenderPassDepthStencilAttachment ds_attachment{};
  if (wgpu.depth_view) {
    ds_attachment.view = wgpu.depth_view;
    if (mode_ & FL_DEPTH) {
      ds_attachment.depthLoadOp     = WGPULoadOp_Clear;
      ds_attachment.depthStoreOp    = WGPUStoreOp_Store;
      ds_attachment.depthClearValue = 1.0f;
    }
    if (mode_ & FL_STENCIL) {
      ds_attachment.stencilLoadOp     = WGPULoadOp_Clear;
      ds_attachment.stencilStoreOp    = WGPUStoreOp_Store;
      ds_attachment.stencilClearValue = 0;
    }
  }

  // Render pass descriptor
  char buf[128];
  snprintf(buf, 128, "Main Render Pass for '%s'",
           label() ? label() : "?");

  WGPURenderPassDescriptor render_pass_desc{};
  render_pass_desc.label                = WGPUStringView{buf, WGPU_STRLEN};
  render_pass_desc.colorAttachmentCount = 1;
  render_pass_desc.colorAttachments     = &color_attachment;
  render_pass_desc.depthStencilAttachment = wgpu.depth_view ? &ds_attachment
                                                            : nullptr;

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
    wgpu.render_pass = 0;
    m_in_render_pass = false;
  }
}

void Fl_WebGPU_Window::draw_overlay()
{
}

/**
   Acquires the next surface texture, creates the command encoder and
   begins the main render pass. If the surface texture cannot be
   acquired, m_frame_active stays false and the frame is skipped.
*/
void Fl_WebGPU_Window::draw_begin()
{
  m_frame_active = false;

  if (!wgpu.device || !pWebGPUWindowDriver ||
      !pWebGPUWindowDriver->acquire_back_buffer())
    return;

  WGPUCommandEncoderDescriptor enc_desc{};
  enc_desc.label = WGPUStringView{"FLTK WebGPU encoder", WGPU_STRLEN};
  wgpu.encoder = wgpuDeviceCreateCommandEncoder(wgpu.device, &enc_desc);
  if (!wgpu.encoder) {
    pWebGPUWindowDriver->release_back_buffer();
    return;
  }

  m_frame_active = true;
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
   To be used as a match for a previous call to Fl_WebGPU_Window::draw_begin().
   Only ends the render pass; the encoder is finished and submitted in
   swap_buffers().
   \see \ref vulkan_with_fltk_widgets
*/
void Fl_WebGPU_Window::draw_end()
{
  end_render_pass();
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

/**
   Finishes the command encoder, submits it to the queue, presents the
   surface and releases the per-frame objects.
*/
void Fl_WebGPU_Window::swap_buffers() {
  if (!m_frame_active)
    return;

  // End all render passes before finishing.
  end_render_pass();

  WGPUCommandBuffer commands = wgpuCommandEncoderFinish(wgpu.encoder, NULL);
  wgpuCommandEncoderRelease(wgpu.encoder);
  wgpu.encoder = 0;

  {
    std::lock_guard<std::mutex> lock(queue_mutex());

    wgpuQueueSubmit(wgpu.queue(), 1, &commands);
    wgpuSurfacePresent(wgpu.surface);
  }

  wgpuCommandBufferRelease(commands);
  pWebGPUWindowDriver->release_back_buffer();
  m_frame_active = false;
}

/**
   Sets the rate at which the window swaps buffers.
   This method can be called after the WebGPU context was created, typically
   within the user overridden `Fl_WebGPU_Window::draw()` method that will be
   overridden by the user.
   \note 0 selects Mailbox (or Immediate) presentation if available,
   a value greater than 0 selects Fifo (vsync). The change is applied
   at the start of the next frame.
   \param[in] value the swap interval
*/
void Fl_WebGPU_Window::swap_interval(int value) {
  if (pWebGPUWindowDriver->swap_interval() != value)
  {
    pWebGPUWindowDriver->swap_interval(value);
  }
}

/**
   Gets the rate at which the window swaps buffers.
   \return an integer greater zero if vertical blanking is taken into account
   when swapping WebGPU buffers
   \return 0 if the vertical blanking is ignored
*/
int Fl_WebGPU_Window::swap_interval() const {
  return pWebGPUWindowDriver->swap_interval();
}

void Fl_WebGPU_Window::invalidate() {
  valid(0);
  context_valid(0);
  if (pWebGPUWindowDriver)
    pWebGPUWindowDriver->invalidate();
}


void Fl_WebGPU_Window::flush() {
  if (!shown() || pixel_w() <= 0 || pixel_h() <= 0)
    return;

  // (Re)create instance/surface/device on first flush and after hide()/show().
  if (!wgpu.device) {
    init_webgpu();
    if (!wgpu.device)
      return;
  }

  draw_begin();
  if (!m_frame_active)
    return;       // surface not ready: skip this frame
  draw();  // User defined virtual draw function
  draw_end();

  // Submit and present
  swap_buffers();
  valid(1);
}


void Fl_WebGPU_Window::resize(int X, int Y, int W, int H) {
  int is_a_resize = (W != Fl_Widget::w() || H != Fl_Widget::h() ||
                     is_a_rescale());

  Fl_Window::resize(X, Y, W, H);

  if (!pWebGPUWindowDriver) pWebGPUWindowDriver = create_driver();
  pWebGPUWindowDriver->resize(is_a_resize, W, H);
  if (is_a_resize)
    valid(0);
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

    void mywindow::draw_begin()
    {
    // Background color
    clearColor = { 0.0, 0.0, 0.0, 0.0 };

    // Clear the screen and prepare m_draw_cmd
    Fl_WebGPU_Window::draw_begin();
    }

    void mywindow::draw() {
         // draw 3d graphics scene
         Fl_WebGPU_Window::draw();
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

void Fl_WebGPU_Window_Driver::invalidate() {}

/** Destroys all WebGPU objects. Safe to call more than once. */
void Fl_WebGPU_Window::shutdown_webgpu() {
  if (!pWebGPUWindowDriver)
      return;

  // Resources first, then the surface (while the native window still
  // exists), then device/queue/adapter/instance.
  if (wgpu.device)
    pWebGPUWindowDriver->destroy_resources();
  pWebGPUWindowDriver->destroy_surface();
  pWebGPUWindowDriver->destroy_device();

  m_in_render_pass = false;
  m_frame_active = false;
  valid(0);
  context_valid(0);
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

/**
   Creates, in order: instance, surface (platform specific), adapter,
   device, queue, surface format/present mode, swapchain and depth buffer.
   Called from flush() when no device exists yet.
*/
void Fl_WebGPU_Window::init_webgpu() {

  if (!pWebGPUWindowDriver)
    pWebGPUWindowDriver = create_driver();

  valid(0);
  context_valid(0);

  // Verify window size
  if (w() <= 0 || h() <= 0)
    return;

  pWebGPUWindowDriver->init_instance();

  // The surface needs the instance and the native (shown) window; the
  // adapter must be picked to be compatible with the surface.
  if (!wgpu.surface)
    pWebGPUWindowDriver->create_surface();
  if (!wgpu.surface)
    Fl::fatal("Could not create WebGPU surface!\n");

  pWebGPUWindowDriver->create_device();

  init_colorspace();

  pWebGPUWindowDriver->prepare();
  context_valid(1);
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
  valid_f_ = 0;
  damage1_ = 0;

  m_in_render_pass = false;
  m_frame_active = false;
  m_debugSync = false;
}

/**
   \}
   \endcond
*/

#endif // HAVE_WEBGPU
