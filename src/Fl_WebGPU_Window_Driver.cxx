//
// Implementation of class Fl_WebGPU_Window_Driver, and of its platform-specific derived classes
// for the Fast Light Tool Kit (FLTK).
//
// Copyright 2016-2018 by Bill Spitzak and others.
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

#include "Fl_WebGPU_Window_Driver.H"
#include <FL/Fl.H>

#include <chrono>
#include <cstdio>
#include <thread>
#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif

// Written against the current webgpu.h (webgpu-native / wgpu-native >= 25,
// Dawn): WGPUStringView labels, callback-info structs, WGPUStatus returns.

#define FL_WGPU_CB_MODE WGPUCallbackMode_AllowSpontaneous

namespace {

struct AdapterRequest { WGPUAdapter adapter = 0; bool done = false; };
struct DeviceRequest  { WGPUDevice  device  = 0; bool done = false; };

void on_adapter(WGPURequestAdapterStatus status, WGPUAdapter adapter,
                WGPUStringView message, void *ud1, void * /*ud2*/)
{
  auto *r = static_cast<AdapterRequest *>(ud1);
  if (status == WGPURequestAdapterStatus_Success)
    r->adapter = adapter;
  else
    fprintf(stderr, "WebGPU: request adapter failed: %.*s\n",
            (int)message.length, message.data ? message.data : "");
  r->done = true;
}

void on_device(WGPURequestDeviceStatus status, WGPUDevice device,
               WGPUStringView message, void *ud1, void * /*ud2*/)
{
  auto *r = static_cast<DeviceRequest *>(ud1);
  if (status == WGPURequestDeviceStatus_Success)
    r->device = device;
  else
    fprintf(stderr, "WebGPU: request device failed: %.*s\n",
            (int)message.length, message.data ? message.data : "");
  r->done = true;
}

void on_uncaptured_error(WGPUDevice const * /*device*/, WGPUErrorType type,
                         WGPUStringView message, void * /*ud1*/, void * /*ud2*/)
{
  fprintf(stderr, "WebGPU error (type %d): %.*s\n", (int)type,
          (int)message.length, message.data ? message.data : "");
}


static int score_surface_format(WGPUTextureFormat fmt)
{
    switch (fmt)
    {
#ifdef __linux__
    case WGPUTextureFormat_BGRA8Unorm:
        return 500;

    case WGPUTextureFormat_RGBA8Unorm:
        return 400;

    case WGPUTextureFormat_BGRA8UnormSrgb:
        return 300;

    case WGPUTextureFormat_RGBA8UnormSrgb:
        return 200;

    default:
        return 0;
#else
    case WGPUTextureFormat_RGBA8Unorm:
        return 500;

    case WGPUTextureFormat_BGRA8Unorm:
        return 400;

    case WGPUTextureFormat_BGRA8UnormSrgb:
        return 300;

    case WGPUTextureFormat_RGBA8UnormSrgb:
        return 200;

    default:
        return 0;
#endif
    }
}

// wgpu-native fires the callback inside the request call; Dawn may need
// the instance to be pumped. This handles both.
// to the browser so the requestAdapter()/requestDevice() promises can resolve.
// Native: wgpu-native fires the callback inside the request call; Dawn may
// need the instance to be pumped.
template <class Req>
bool wait_for(WGPUInstance instance, WGPUFuture /*future*/, Req &req)
{
#ifdef __EMSCRIPTEN__
  for (int i = 0; i < 3000 && !req.done; ++i) {   // ~30 s
    wgpuInstanceProcessEvents(instance);
    if (!req.done)
      emscripten_sleep(10);
  }
#else
  for (int i = 0; i < 5000 && !req.done; ++i) {
    wgpuInstanceProcessEvents(instance);
    if (!req.done)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
#endif
  return req.done;
}

} // namespace


void Fl_WebGPU_Window_Driver::init_instance()
{
  Fl_WebGPU_Context &w = pWindow->wgpu;
  if (w.instance)
    return;
  w.instance = wgpuCreateInstance(NULL);
  if (!w.instance)
    Fl::fatal("Could not initialize WebGPU instance\n");
}

// Needs wgpu.instance and wgpu.surface (the adapter must be able to present
// to our surface).
void Fl_WebGPU_Window_Driver::create_device()
{
  Fl_WebGPU_Context &w = pWindow->wgpu;
  if (w.device)
    return;

#ifdef __EMSCRIPTEN__
  if (!EM_ASM_INT({ return (typeof navigator !== 'undefined' && navigator.gpu) ? 1 : 0; }))
    Fl::fatal("WebGPU is not available in this browser (navigator.gpu is "
              "undefined). Use a WebGPU-enabled browser and serve the page "
              "from https:// or http://localhost.\n");
#endif

  // --- Adapter ---
  if (!w.adapter) {
    AdapterRequest req;
    WGPURequestAdapterOptions opts = {};
#ifndef __EMSCRIPTEN__
    opts.compatibleSurface = w.surface;   // not meaningful in the browser
#endif

    WGPURequestAdapterCallbackInfo cb = {};
    cb.mode      = FL_WGPU_CB_MODE;
    cb.callback  = on_adapter;
    cb.userdata1 = &req;
    WGPUFuture fut = wgpuInstanceRequestAdapter(w.instance, &opts, cb);

    if (!wait_for(w.instance, fut, req) || !req.adapter)
      Fl::fatal("Could not initialize WebGPU adapter!\n");
    w.adapter = req.adapter;
  }

  // --- Device ---
  {
    DeviceRequest req;
    WGPUDeviceDescriptor desc = {};
    desc.label = WGPUStringView{"FLTK WebGPU device", WGPU_STRLEN};
    desc.uncapturedErrorCallbackInfo.callback = on_uncaptured_error;

    WGPURequestDeviceCallbackInfo cb = {};
    cb.mode      = FL_WGPU_CB_MODE;
    cb.callback  = on_device;
    cb.userdata1 = &req;
    WGPUFuture fut = wgpuAdapterRequestDevice(w.adapter, &desc, cb);

    if (!wait_for(w.instance, fut, req) || !req.device)
      Fl::fatal("Could not initialize WebGPU device!\n");
    w.device = req.device;
  }

  // --- Queue ---
  w.queue() = wgpuDeviceGetQueue(w.device);
  if (!w.queue())
    Fl::fatal("Could not initialize WebGPU queue!\n");
}

// Picks the surface format and the present mode from what the
// surface/adapter pair supports.
void Fl_WebGPU_Window_Driver::init_colorspace()
{
  Fl_WebGPU_Context &w = pWindow->wgpu;
  if (!w.surface || !w.adapter)
    return;

  WGPUSurfaceCapabilities caps = {};
  if (wgpuSurfaceGetCapabilities(w.surface, w.adapter, &caps) == WGPUStatus_Success) {
      int bestScore = -1;

      for (size_t i = 0; i < caps.formatCount; ++i)
      {
        const WGPUTextureFormat fmt = caps.formats[i];
        const int score = score_surface_format(fmt);

        if (score > bestScore)
        {
          bestScore = score;
          w.surface_format = fmt;
        }
      }

      wgpuSurfaceCapabilitiesFreeMembers(caps);
  }
  apply_present_mode();
}

// swap_interval 0: MAILBOX (else IMMEDIATE) if available; >0: FIFO (vsync).
void Fl_WebGPU_Window_Driver::apply_present_mode()
{
  Fl_WebGPU_Context &w = pWindow->wgpu;
  present_mode_ = WGPUPresentMode_Fifo;   // always supported

  if (swap_interval_ <= 0 && w.surface && w.adapter) {
    WGPUSurfaceCapabilities caps = {};
    if (wgpuSurfaceGetCapabilities(w.surface, w.adapter, &caps) == WGPUStatus_Success) {
      bool mailbox = false, immediate = false;
      for (size_t i = 0; i < caps.presentModeCount; ++i) {
        if (caps.presentModes[i] == WGPUPresentMode_Mailbox)   mailbox = true;
        if (caps.presentModes[i] == WGPUPresentMode_Immediate) immediate = true;
      }
      if (mailbox)         present_mode_ = WGPUPresentMode_Mailbox;
      else if (immediate)  present_mode_ = WGPUPresentMode_Immediate;
      wgpuSurfaceCapabilitiesFreeMembers(caps);
    }
  }
  needs_configure_ = true;   // applied at the next acquire_back_buffer()
}

void Fl_WebGPU_Window_Driver::swap_interval(int x)
{
  if (x == swap_interval_)
    return;
  swap_interval_ = x;
  apply_present_mode();
}

// (Re)configures the surface: the WebGPU equivalent of recreating the
// swapchain.
void Fl_WebGPU_Window_Driver::prepare_buffers()
{
  Fl_WebGPU_Context &w = pWindow->wgpu;
  if (!w.surface || !w.device)
    return;

  int W = pWindow->pixel_w();
  int H = pWindow->pixel_h();
  if (W <= 0 || H <= 0)
    return;

  WGPUSurfaceConfiguration cfg = {};
  cfg.device      = w.device;
  cfg.format      = w.surface_format;
  cfg.usage       = WGPUTextureUsage_RenderAttachment;
  cfg.width       = (uint32_t)W;
  cfg.height      = (uint32_t)H;
  cfg.alphaMode   = WGPUCompositeAlphaMode_Auto;
  cfg.presentMode = present_mode_;
  wgpuSurfaceConfigure(w.surface, &cfg);

  configured_w_ = W;
  configured_h_ = H;
  needs_configure_ = false;
}

// Prepare depth/stencil resources for window
void Fl_WebGPU_Window_Driver::prepare_depth()
{
  Fl_WebGPU_Context &w = pWindow->wgpu;
  destroy_depth();   // recreate from scratch (e.g. on resize)

  bool has_depth   = pWindow->mode() & FL_DEPTH;
  bool has_stencil = pWindow->mode() & FL_STENCIL;

  if (!has_depth && !has_stencil)
    return;
  if (!w.device || configured_w_ <= 0 || configured_h_ <= 0)
    return;

  // Vulkan D16 / D24S8 / S8 mapping. Depth24Plus is used instead of
  // Depth16Unorm because it is universally supported and far more precise.
  if (has_stencil)
    w.depth_format = has_depth ? WGPUTextureFormat_Depth24PlusStencil8
                               : WGPUTextureFormat_Stencil8;
  else
    w.depth_format = WGPUTextureFormat_Depth24Plus;

  WGPUTextureDescriptor td = {};
  td.label         = WGPUStringView{"FLTK depth/stencil", WGPU_STRLEN};
  td.usage         = WGPUTextureUsage_RenderAttachment;
  td.dimension     = WGPUTextureDimension_2D;
  td.size          = { (uint32_t)configured_w_, (uint32_t)configured_h_, 1 };
  td.format        = w.depth_format;
  td.mipLevelCount = 1;
  td.sampleCount   = 1;

  w.depth_texture = wgpuDeviceCreateTexture(w.device, &td);
  if (!w.depth_texture) {
    fprintf(stderr, "wgpuDeviceCreateTexture (depth) failed\n");
    w.depth_format = WGPUTextureFormat_Undefined;
    return;
  }

  WGPUTextureViewDescriptor vd = {};
  vd.label           = WGPUStringView{"FLTK depth/stencil view", WGPU_STRLEN};
  vd.format          = w.depth_format;
  vd.dimension       = WGPUTextureViewDimension_2D;
  vd.baseMipLevel    = 0;
  vd.mipLevelCount   = 1;
  vd.baseArrayLayer  = 0;
  vd.arrayLayerCount = 1;
  vd.aspect          = WGPUTextureAspect_All;
  w.depth_view = wgpuTextureCreateView(w.depth_texture, &vd);
}

void Fl_WebGPU_Window_Driver::destroy_depth()
{
  Fl_WebGPU_Context &w = pWindow->wgpu;
  if (w.depth_view) {
    wgpuTextureViewRelease(w.depth_view);
    w.depth_view = 0;
  }
  if (w.depth_texture) {
    wgpuTextureDestroy(w.depth_texture);
    wgpuTextureRelease(w.depth_texture);
    w.depth_texture = 0;
  }
  w.depth_format = WGPUTextureFormat_Undefined;
}

void Fl_WebGPU_Window_Driver::prepare()
{
  prepare_buffers();
  prepare_depth();
  pWindow->prepare();
}

void Fl_WebGPU_Window_Driver::resize(int is_a_resize, int /*w*/, int /*h*/)
{
  // Arguments are in FLTK units; prepare_buffers() uses pixel_w()/pixel_h().
  if (!is_a_resize || !pWindow->wgpu.device || !pWindow->wgpu.surface)
    return;
  prepare_buffers();
  prepare_depth();
}

bool Fl_WebGPU_Window_Driver::acquire_back_buffer()
{
  Fl_WebGPU_Context &w = pWindow->wgpu;
  if (!w.surface || !w.device)
    return false;

  int W = pWindow->pixel_w();
  int H = pWindow->pixel_h();
  if (W <= 0 || H <= 0)
    return false;

  const bool resized = (W != configured_w_ || H != configured_h_);
  if (resized || needs_configure_)
    prepare_buffers();
  if (resized)
    prepare_depth();

  WGPUSurfaceTexture st = {};
  wgpuSurfaceGetCurrentTexture(w.surface, &st);

  switch (st.status) {
    case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal:
      break;
    case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal:
      needs_configure_ = true;       // usable now, reconfigure next frame
      break;
    case WGPUSurfaceGetCurrentTextureStatus_Timeout:
    case WGPUSurfaceGetCurrentTextureStatus_Outdated:
    case WGPUSurfaceGetCurrentTextureStatus_Lost:
      if (st.texture) wgpuTextureRelease(st.texture);
      needs_configure_ = true;
      pWindow->redraw();             // retry
      return false;
    default:
      fprintf(stderr, "WebGPU: wgpuSurfaceGetCurrentTexture failed (status %d)\n",
              (int)st.status);
      if (st.texture) wgpuTextureRelease(st.texture);
      return false;
  }

  w.surface_texture = st;
  w.back_buffer = wgpuTextureCreateView(st.texture, NULL);
  if (!w.back_buffer) {
    release_back_buffer();
    return false;
  }
  return true;
}

void Fl_WebGPU_Window_Driver::release_back_buffer()
{
  Fl_WebGPU_Context &w = pWindow->wgpu;
  if (w.back_buffer) {
    wgpuTextureViewRelease(w.back_buffer);
    w.back_buffer = 0;
  }
  if (w.surface_texture.texture) {
    wgpuTextureRelease(w.surface_texture.texture);
    w.surface_texture.texture = 0;
  }
}

void Fl_WebGPU_Window_Driver::destroy_surface()
{
  Fl_WebGPU_Context &w = pWindow->wgpu;
  release_back_buffer();
  if (w.surface) {
    wgpuSurfaceUnconfigure(w.surface);
    wgpuSurfaceRelease(w.surface);
    w.surface = 0;
  }
  configured_w_ = configured_h_ = 0;
}

// Destroys resources in reverse creation order (first, those of window)
void Fl_WebGPU_Window_Driver::destroy_resources()
{
    if (!pWindow || !pWindow->wgpu.device)
    {
        fprintf(stderr, "destroy_resources: Invalid device\n");
        return;
    }
    Fl_WebGPU_Context &w = pWindow->wgpu;

    // A frame may be half-recorded if we are hidden during draw().
    if (pWindow->m_in_render_pass)
        pWindow->end_render_pass();
    if (w.encoder) {
        wgpuCommandEncoderRelease(w.encoder);
        w.encoder = 0;
    }
    pWindow->m_frame_active = false;
    release_back_buffer();

    if (w.pipeline) {
        wgpuRenderPipelineRelease(w.pipeline);
        w.pipeline = 0;
    }
    destroy_depth();
}

// Call after destroy_resources() and destroy_surface().
void Fl_WebGPU_Window_Driver::destroy_device()
{
    Fl_WebGPU_Context &w = pWindow->wgpu;
    if (w.queue())  { wgpuQueueRelease(w.queue());     w.queue()  = 0; }
    if (w.device)   { wgpuDeviceRelease(w.device);     w.device   = 0; }
    if (w.adapter)  { wgpuAdapterRelease(w.adapter);   w.adapter  = 0; }
    if (w.instance) { wgpuInstanceRelease(w.instance); w.instance = 0; }
}

Fl_WebGPU_Window_Driver::Fl_WebGPU_Window_Driver(Fl_WebGPU_Window* win) :
    pWindow(win)
{
    swap_interval_ = 0;  // MAILBOX as a default presentation mode
}

Fl_WebGPU_Window_Driver::~Fl_WebGPU_Window_Driver()
{
}

#endif // HAVE_WEBGPU
