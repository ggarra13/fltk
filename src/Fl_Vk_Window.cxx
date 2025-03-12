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

#if HAVE_VK

extern int fl_vk_load_plugin;


#include <FL/vk.h>
#include <FL/Fl_Vk_Window.H>  // \@done
#include "Fl_Vk_Window_Driver.H"
#include "Fl_Window_Driver.H"  // \@done
#include <FL/Fl_Graphics_Driver.H> // \@done
#include <FL/fl_utf8.h>            // \@done
// #include "drivers/Vulkan/Fl_Vulkan_Display_Device.H"
// #include "drivers/Vulkan/Fl_Vulkan_Graphics_Driver.H"

#include <stdlib.h>
#  if (HAVE_DLSYM && HAVE_DLFCN_H)
#    include <dlfcn.h>
#  endif // (HAVE_DLSYM && HAVE_DLFCN_H)


static void demo_draw_build_cmd(Fl_Vk_Window* pWindow) {
    VkCommandBufferBeginInfo cmd_buf_info = {};
    cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buf_info.pNext = NULL;
    cmd_buf_info.flags = 0;
    cmd_buf_info.pInheritanceInfo = NULL;
    
    VkClearValue clear_values[2];

    VkClearValue color;
    color.color = {0.2f, 0.5f, 0.2f, 0.2f};
    
    VkClearValue depth;
    depth.depthStencil = {pWindow->m_depthStencil, 0}; // \@todo:
    
    clear_values[0] = color;
    clear_values[1] = depth;
    
    VkRenderPassBeginInfo rp_begin = {};
    rp_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin.pNext = NULL;
    rp_begin.renderPass = pWindow->m_renderPass;
    rp_begin.framebuffer = pWindow->m_framebuffers[pWindow->m_current_buffer];
    rp_begin.renderArea.offset.x = 0;
    rp_begin.renderArea.offset.y = 0;
    rp_begin.renderArea.extent.width = pWindow->w();
    rp_begin.renderArea.extent.height = pWindow->h();
    rp_begin.clearValueCount = 2;
    rp_begin.pClearValues = clear_values;
    
    VkResult result;

    result = vkBeginCommandBuffer(pWindow->m_draw_cmd, &cmd_buf_info);
    VK_CHECK_RESULT(result);

    // We can use LAYOUT_UNDEFINED as a wildcard here because we don't care what
    // happens to the previous contents of the image
    VkImageMemoryBarrier image_memory_barrier = {};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.pNext = NULL;
    image_memory_barrier.srcAccessMask = 0;
    image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.image = pWindow->m_buffers[pWindow->m_current_buffer].image;
    image_memory_barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(pWindow->m_draw_cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0,
                         NULL, 1, &image_memory_barrier);
    vkCmdBeginRenderPass(pWindow->m_draw_cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(pWindow->m_draw_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pWindow->m_pipeline);
    vkCmdBindDescriptorSets(pWindow->m_draw_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pWindow->m_pipeline_layout, 0, 1, &pWindow->m_desc_set, 0,
                            NULL);

    VkViewport viewport;
    memset(&viewport, 0, sizeof(viewport));
    viewport.width = (float)pWindow->w();
    viewport.height = (float)pWindow->h();
    viewport.minDepth = (float)0.0f;
    viewport.maxDepth = (float)1.0f;
    vkCmdSetViewport(pWindow->m_draw_cmd, 0, 1, &viewport);

    VkRect2D scissor;
    memset(&scissor, 0, sizeof(scissor));
    scissor.extent.width = pWindow->w();
    scissor.extent.height = pWindow->h();
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    vkCmdSetScissor(pWindow->m_draw_cmd, 0, 1, &scissor);

    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(pWindow->m_draw_cmd, VERTEX_BUFFER_BIND_ID, 1,
                           &pWindow->m_vertices.buf, offsets);

    vkCmdDraw(pWindow->m_draw_cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(pWindow->m_draw_cmd);

    VkImageMemoryBarrier prePresentBarrier = {};
    prePresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    prePresentBarrier.pNext = NULL;
    prePresentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    prePresentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    prePresentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    prePresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    prePresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    prePresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    prePresentBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    prePresentBarrier.image = pWindow->m_buffers[pWindow->m_current_buffer].image;
    VkImageMemoryBarrier *pmemory_barrier = &prePresentBarrier;
    vkCmdPipelineBarrier(pWindow->m_draw_cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0,
                         NULL, 1, pmemory_barrier);

    result = vkEndCommandBuffer(pWindow->m_draw_cmd);
    VK_CHECK_RESULT(result);
}

static void demo_flush_init_cmd(Fl_Vk_Window* pWindow) {

    VkResult result;
    
    if (pWindow->m_setup_cmd == VK_NULL_HANDLE)
        return;

    result = vkEndCommandBuffer(pWindow->m_setup_cmd);
    VK_CHECK_RESULT(result);

    const VkCommandBuffer cmd_bufs[] = {pWindow->m_setup_cmd};
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.waitSemaphoreCount = 0;
    submit_info.pWaitSemaphores = NULL;
    submit_info.pWaitDstStageMask = NULL;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = cmd_bufs;
    submit_info.signalSemaphoreCount = 0;
    submit_info.pSignalSemaphores = NULL;

    result = vkQueueSubmit(pWindow->m_queue, 1, &submit_info,
                        pWindow->m_renderFence);
    VK_CHECK_RESULT(result);

    result = vkQueueWaitIdle(pWindow->m_queue);
    VK_CHECK_RESULT(result);

    vkFreeCommandBuffers(pWindow->m_device, pWindow->m_cmd_pool, 1, cmd_bufs);
    pWindow->m_setup_cmd = VK_NULL_HANDLE;
}


static void demo_draw(Fl_Vk_Window* pWindow) {
    VkResult result;
    VkSemaphore imageAcquiredSemaphore, drawCompleteSemaphore;
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = NULL;
    semaphoreCreateInfo.flags = 0;

    result = vkCreateSemaphore(pWindow->m_device, &semaphoreCreateInfo,
                               NULL, &imageAcquiredSemaphore);
    VK_CHECK_RESULT(result);

    result = vkCreateSemaphore(pWindow->m_device, &semaphoreCreateInfo,
                               NULL, &drawCompleteSemaphore);
    VK_CHECK_RESULT(result);

    // Get the index of the next available swapchain image:
    result = vkAcquireNextImageKHR(pWindow->m_device,
                                   pWindow->m_swapchain, UINT64_MAX,
                                   imageAcquiredSemaphore,
                                   (VkFence)0, // TODO: Show use of fence
                                   &pWindow->m_current_buffer);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // pWindow->m_swapchain is out of date (e.g. the window was resized) and
        // must be recreated:
        Fl_Vk_Window_Driver::driver(pWindow)->resize(true, pWindow->w(), pWindow->h());
        demo_draw(pWindow);
        vkDestroySemaphore(pWindow->m_device, imageAcquiredSemaphore, NULL);
        vkDestroySemaphore(pWindow->m_device, drawCompleteSemaphore, NULL);
        return;
    } else if (result == VK_SUBOPTIMAL_KHR) {
        // pWindow->m_swapchain is not as optimal as it could be, but the platform's
        // presentation engine will still present the image correctly.
    } else {
        VK_CHECK_RESULT(result);
    }

    demo_flush_init_cmd(pWindow);

    // Wait for the present complete semaphore to be signaled to ensure
    // that the image won't be rendered to until the presentation
    // engine has fully released ownership to the application, and it is
    // okay to render to the image.

    demo_draw_build_cmd(pWindow);
    VkFence nullFence = VK_NULL_HANDLE;
    VkPipelineStageFlags pipe_stage_flags =
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &imageAcquiredSemaphore;
    submit_info.pWaitDstStageMask = &pipe_stage_flags;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &pWindow->m_draw_cmd;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &drawCompleteSemaphore;

    result = vkQueueSubmit(pWindow->m_queue, 1, &submit_info, nullFence);
    VK_CHECK_RESULT(result);

    VkPresentInfoKHR present = {};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.pNext = NULL;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &drawCompleteSemaphore;
    present.swapchainCount = 1;
    present.pSwapchains = &pWindow->m_swapchain;
    present.pImageIndices = &pWindow->m_current_buffer;

    result = vkQueuePresentKHR(pWindow->m_queue, &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // pWindow->m_swapchain is out of date (e.g. the window was resized) and
        // must be recreated:
        Fl_Vk_Window_Driver::driver(pWindow)->resize(true, pWindow->w(), pWindow->h());
    } else if (result == VK_SUBOPTIMAL_KHR) {
        // pWindow->m_swapchain is not as optimal as it could be, but the platform's
        // presentation engine will still present the image correctly.
    } else {
        VK_CHECK_RESULT(result);
    }

    result = vkQueueWaitIdle(pWindow->m_queue);
    VK_CHECK_RESULT(result);

    vkDestroySemaphore(pWindow->m_device, imageAcquiredSemaphore, NULL);
    vkDestroySemaphore(pWindow->m_device, drawCompleteSemaphore, NULL);
}

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
  return Fl_Vk_Window_Driver::global()->find(a,b) != 0;
}

void Fl_Vk_Window::show() {
  int need_after = 0;
  if (!shown()) {
    Fl_Window::default_size_range();
    if (!g) {
      g = pVkWindowDriver->find(mode_,alist);
      if (!g && (mode_ & FL_DOUBLE) == FL_SINGLE) {
        g = pVkWindowDriver->find(mode_ | FL_DOUBLE,alist);
        if (g) mode_ |= FL_FAKE_SINGLE;
       }

      if (!g) {
        Fl::error("Insufficient Vulkan support");
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
  pVkWindowDriver->invalidate();
}

int Fl_Vk_Window::mode(int m, const int *a) {
  if (m == mode_ && a == alist) return 0;
  return pVkWindowDriver->mode_(m, a);
}

/**
  The make_current() method selects the Vulkan context for the
  widget.  It is called automatically prior to the draw() method
  being called and can also be used to implement feedback and/or
  selection within the handle() method.
*/

void Fl_Vk_Window::make_current() {
    pVkWindowDriver->make_current_before();
    if (m_surface == VK_NULL_HANDLE)
    {
        pVkWindowDriver->init_vk();
        pVkWindowDriver->create_surface();
        pVkWindowDriver->init_vk_swapchain();
        pVkWindowDriver->prepare();
        valid(0);
        context_valid(0);
    }
    pVkWindowDriver->make_current_after();
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
      swap_buffers();
    } else if (SWAP_TYPE == UNDEFINED){ // SWAP_TYPE == UNDEFINED
        damage1_ = damage();
        clear_damage(0xff); draw();
        swap_buffers();
    }

  } else {      // single-buffered context is simpler:

    draw();

  }

  valid(1);
}

void Fl_Vk_Window::resize(int X,int Y,int W,int H) {
 // printf("Fl_Vk_Window::resize(X=%d, Y=%d, W=%d, H=%d)\n", X, Y, W, H);
 // printf("current: x()=%d, y()=%d, w()=%d, h()=%d\n", x(), y(), w(), h());
    
  int is_a_resize = (W != Fl_Widget::w() || H != Fl_Widget::h() || is_a_rescale());
  if (is_a_resize) {
      valid(0);
  }
  pVkWindowDriver->resize(is_a_resize, W, H);
  Fl_Window::resize(X,Y,W,H);
}

/**
  Hides the window and destroys the Vulkan context.
*/
void Fl_Vk_Window::hide() {
  Fl_Window::hide();
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

/**
 Supports drawing to an Fl_Vk_Window with the FLTK 2D drawing API.
 \see \ref opengl_with_fltk_widgets
 */
void Fl_Vk_Window::draw_begin() {
}

/**
 To be used as a match for a previous call to Fl_Vk_Window::draw_begin().
 \see \ref opengl_with_fltk_widgets
 */
void Fl_Vk_Window::draw_end() {
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
     ... draw your geometry here ...
    }
  \endcode

  Actual example code to clear screen to black and draw a 2D white "X":
  \code
    void mywindow::draw() {
    Fl_Vk_Window::draw();
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
void Fl_Vk_Window::draw()
{
    VK_CHECK_HANDLE(m_instance);
    VK_CHECK_HANDLE(m_surface);
    VK_CHECK_HANDLE(m_gpu);
    VK_CHECK_HANDLE(m_device);
    VK_CHECK_HANDLE(m_swapchain);
    VK_CHECK_HANDLE(m_queue);
    
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
}


char Fl_Vk_Window_Driver::swap_type() {return UNDEFINED;}


Fl_Font_Descriptor** Fl_Vk_Window_Driver::fontnum_to_fontdescriptor(int fnum) {
  extern FL_EXPORT Fl_Fontdesc *fl_fonts;
  return &(fl_fonts[fnum].first);
}

/* Captures a rectangle of a Fl_Vk_Window and returns it as an RGB image.
 This is the platform-independent version. Some platforms may override it.
 */
Fl_RGB_Image* Fl_Vk_Window_Driver::capture_vk_rectangle(int x, int y, int w, int h)
{
    return nullptr;
}

/**
  The destructor removes the widget and destroys the Vulkan context
  associated with it.
*/
Fl_Vk_Window::~Fl_Vk_Window() {
  hide();
  delete pVkWindowDriver;
}

void Fl_Vk_Window::init() {
  pVkWindowDriver = Fl_Vk_Window_Driver::newVkWindowDriver(this);
  end(); // we probably don't want any children
  box(FL_NO_BOX);

  mode_    = FL_RGB | FL_DEPTH | FL_DOUBLE;
  alist    = 0;
  g        = 0;
  valid_f_ = 0;
  damage1_ = 0;

  // Set up defaults
  m_width = w();
  m_height = h();
  
  // Reset Vulkan Handles
  m_device     = VK_NULL_HANDLE;
  m_gpu        = VK_NULL_HANDLE;;
  m_surface    = VK_NULL_HANDLE;
  m_swapchain  = VK_NULL_HANDLE;
  m_renderPass = VK_NULL_HANDLE;
  m_pipeline   = VK_NULL_HANDLE;
  m_allocator  = nullptr;

  // These are specific to GLFW demo
  m_setup_cmd  = VK_NULL_HANDLE;
  m_draw_cmd   = VK_NULL_HANDLE;
  m_cmd_pool   = VK_NULL_HANDLE;
  m_desc_pool  = VK_NULL_HANDLE;
  m_pipeline_layout = VK_NULL_HANDLE;
  m_desc_layout = VK_NULL_HANDLE;

  // Pointers for ImGUI (not used for now)
  // m_frames          = nullptr;
  // m_frameSemaphores = nullptr;
  // m_framebuffers    = nullptr;
  // m_semaphoreCount  = 0;

  // These are specific to GLFW demo
  m_buffers         = nullptr;

  // Enums
  // m_presentMode     = VK_PRESENT_MODE_IMMEDIATE_KHR;
  // m_curFrame            = 0;
  

  // Counters
  m_clearEnable         = true;
  m_swapchainImageCount = 0;
  m_queueFamilyIndex    = 0;
  m_current_buffer      = 0;
}

/**
 \}
 \endcond
 */

#endif // HAVE_VK
