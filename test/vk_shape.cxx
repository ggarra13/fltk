//
// Tiny Vulkan demo program for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2010 by Bill Spitzak and others.
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
#include <FL/platform.H>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Hor_Slider.H>
#include <FL/math.h>

#if HAVE_VK

#include <FL/Fl_Vk_Window.H>

class vk_shape_window : public Fl_Vk_Window {
    void draw() FL_OVERRIDE;
public:
    int sides;
    vk_shape_window(int x,int y,int w,int h,const char *l=0);
    vk_shape_window(int w,int h,const char *l=0);

    void prepare() FL_OVERRIDE;

protected:
    void prepare_textures();

private:
    void prepare_texture_image(const uint32_t *tex_colors,
                               Fl_Vk_Texture* tex_obj,
                               VkImageTiling tiling,
                               VkImageUsageFlags usage,
                               VkFlags required_props);
    void set_image_layout(VkImage image,
                          VkImageAspectFlags aspectMask,
                          VkImageLayout old_image_layout,
                          VkImageLayout new_image_layout,
                          int srcAccessMaskInt);
};

vk_shape_window::vk_shape_window(int x,int y,int w,int h,const char *l) :
Fl_Vk_Window(x,y,w,h,l) {
    mode(FL_RGB | FL_DOUBLE | FL_ALPHA);
  sides = 3;
}

vk_shape_window::vk_shape_window(int w,int h,const char *l) :
Fl_Vk_Window(w,h,l) {
  sides = 3;
}

// needed
static bool memory_type_from_properties(Fl_Vk_Window* pWindow,
                                        uint32_t typeBits,
                                        VkFlags requirements_mask,
                                        uint32_t *typeIndex) {
    uint32_t i;
    // Search memtypes to find first index with those properties
    for (i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
        if ((typeBits & 1) == 1) {
            // Type is available, does it match user properties?
            if ((pWindow->m_memory_properties.memoryTypes[i].propertyFlags &
                 requirements_mask) == requirements_mask) {
                *typeIndex = i;
                return true;
            }
        }
        typeBits >>= 1;
    }
    // No memory types matched, return failure
    return false;
}

// Uses m_cmd_pool, m_setup_cmd
void vk_shape_window::set_image_layout(VkImage image,
                                       VkImageAspectFlags aspectMask,
                                       VkImageLayout old_image_layout,
                                       VkImageLayout new_image_layout,
                                       int srcAccessMaskInt)
{
    VkResult err;

    VkAccessFlagBits srcAccessMask = static_cast<VkAccessFlagBits>(srcAccessMaskInt);
    if (m_setup_cmd == VK_NULL_HANDLE) {
        VkCommandBufferAllocateInfo cmd = {};
        cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd.pNext = NULL;
        cmd.commandPool = m_cmd_pool;
        cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd.commandBufferCount = 1;
        
        err = vkAllocateCommandBuffers(m_device, &cmd,
                                       &m_setup_cmd);
        VkCommandBufferBeginInfo cmd_buf_info = {};
        cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_buf_info.pNext = NULL;
        cmd_buf_info.flags = 0;
        cmd_buf_info.pInheritanceInfo = NULL;
        
        err = vkBeginCommandBuffer(m_setup_cmd, &cmd_buf_info);
    }

    VkImageMemoryBarrier image_memory_barrier = {};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.pNext = NULL;
    image_memory_barrier.srcAccessMask = srcAccessMask;
    image_memory_barrier.dstAccessMask = 0;
    image_memory_barrier.oldLayout = old_image_layout;
    image_memory_barrier.newLayout = new_image_layout;
    image_memory_barrier.image = image;
    image_memory_barrier.subresourceRange = {aspectMask, 0, 1, 0, 1};

    VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_HOST_BIT; // Default for host writes
    VkPipelineStageFlags dest_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // Default

    // Adjust source and destination stages based on access and layout
    if (srcAccessMask & VK_ACCESS_HOST_WRITE_BIT) {
        src_stages = VK_PIPELINE_STAGE_HOST_BIT;
    } else if (srcAccessMask & VK_ACCESS_TRANSFER_WRITE_BIT) {
        src_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

    if (new_image_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dest_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (new_image_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dest_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (new_image_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        image_memory_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dest_stages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else if (new_image_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dest_stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(m_setup_cmd,
                         src_stages, dest_stages, 0, 0, NULL,
                         0, NULL, 1, &image_memory_barrier);
}


// Uses m_device, m_setup_cmd, m_queue, m_cmd_pool
static void demo_flush_init_cmd(Fl_Vk_Window* pWindow) {
    VkResult result;

    if (pWindow->m_setup_cmd == VK_NULL_HANDLE)
        return;

    result = vkEndCommandBuffer(pWindow->m_setup_cmd);
    VK_CHECK_RESULT(result);

    const VkCommandBuffer cmd_bufs[] = {pWindow->m_setup_cmd};
    VkFence nullFence = {VK_NULL_HANDLE};
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

    result = vkQueueSubmit(pWindow->m_queue, 1, &submit_info, nullFence);
    VK_CHECK_RESULT(result);

    result = vkQueueWaitIdle(pWindow->m_queue);
    VK_CHECK_RESULT(result);

    vkFreeCommandBuffers(pWindow->m_device, pWindow->m_cmd_pool, 1, cmd_bufs);
    pWindow->m_setup_cmd = VK_NULL_HANDLE;
}

void vk_shape_window::prepare_texture_image(const uint32_t *tex_colors,
                                            Fl_Vk_Texture* tex_obj,
                                            VkImageTiling tiling,
                                            VkImageUsageFlags usage,
                                            VkFlags required_props) {
    const VkFormat tex_format = VK_FORMAT_B8G8R8A8_UNORM;
    const int32_t tex_width = 2;
    const int32_t tex_height = 2;
    VkResult result;
    bool pass;

    tex_obj->tex_width = tex_width;
    tex_obj->tex_height = tex_height;

    VkImageCreateInfo image_create_info = {};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.pNext = NULL;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = tex_format;
    image_create_info.extent = {tex_width, tex_height, 1};
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = tiling;
    image_create_info.usage = usage;
    image_create_info.flags = 0;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    
    VkMemoryAllocateInfo mem_alloc = {};
    mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_alloc.pNext = NULL;
    mem_alloc.allocationSize = 0;
    mem_alloc.memoryTypeIndex = 0;

    VkMemoryRequirements mem_reqs;

    result =
        vkCreateImage(m_device, &image_create_info, NULL, &tex_obj->image);
    VK_CHECK_RESULT(result);

    vkGetImageMemoryRequirements(m_device, tex_obj->image, &mem_reqs);

    mem_alloc.allocationSize = mem_reqs.size;
    pass =
        memory_type_from_properties(this, mem_reqs.memoryTypeBits,
                                    required_props, &mem_alloc.memoryTypeIndex);

    /* allocate memory */
    result = vkAllocateMemory(m_device, &mem_alloc, NULL, &tex_obj->mem);
    VK_CHECK_RESULT(result);

    /* bind memory */
    result = vkBindImageMemory(m_device, tex_obj->image, tex_obj->mem, 0);
    VK_CHECK_RESULT(result);

    if (required_props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        VkImageSubresource subres = {};
        subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subres.mipLevel = 0;
        subres.arrayLayer = 0;
        VkSubresourceLayout layout;
        void *data;
        int32_t x, y;

        vkGetImageSubresourceLayout(m_device, tex_obj->image, &subres,
                                    &layout);

        result = vkMapMemory(m_device, tex_obj->mem, 0,
                             mem_alloc.allocationSize, 0, &data);
        VK_CHECK_RESULT(result);

        for (y = 0; y < tex_height; y++) {
            uint32_t *row = (uint32_t *)((char *)data + layout.rowPitch * y);
            for (x = 0; x < tex_width; x++)
                row[x] = tex_colors[(x & 1) ^ (y & 1)];
        }

        vkUnmapMemory(m_device, tex_obj->mem);
    }

    tex_obj->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    set_image_layout(tex_obj->image, VK_IMAGE_ASPECT_COLOR_BIT,
                     VK_IMAGE_LAYOUT_PREINITIALIZED, tex_obj->imageLayout,
                     VK_ACCESS_HOST_WRITE_BIT);
    /* setting the image layout does not reference the actual memory so no need
     * to add a mem ref */
}


void vk_shape_window::prepare_textures()
{
    const VkFormat tex_format = VK_FORMAT_B8G8R8A8_UNORM;
    VkFormatProperties props;
    const uint32_t tex_colors[DEMO_TEXTURE_COUNT][2] = {
        {0xffff0000, 0xff00ff00},
    };
    uint32_t i;
    VkResult result;

    vkGetPhysicalDeviceFormatProperties(m_gpu, tex_format, &props);

    for (i = 0; i < DEMO_TEXTURE_COUNT; i++) {
        if ((props.linearTilingFeatures &
             VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) &&
            !m_use_staging_buffer) {
            /* Device can texture using linear textures */
            prepare_texture_image(
                tex_colors[i], &m_textures[i], VK_IMAGE_TILING_LINEAR,
                VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        } else if (props.optimalTilingFeatures &
                   VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            /* Must use staging buffer to copy linear texture to optimized */
            Fl_Vk_Texture staging_texture;

            memset(&staging_texture, 0, sizeof(staging_texture));
            prepare_texture_image(
                tex_colors[i], &staging_texture,
                VK_IMAGE_TILING_LINEAR,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            prepare_texture_image(
                tex_colors[i], &m_textures[i],
                VK_IMAGE_TILING_OPTIMAL,
                (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            set_image_layout(staging_texture.image,
                             VK_IMAGE_ASPECT_COLOR_BIT,
                             staging_texture.imageLayout,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             0);

            set_image_layout(m_textures[i].image,
                             VK_IMAGE_ASPECT_COLOR_BIT,
                             m_textures[i].imageLayout,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             0);

            VkImageCopy copy_region = {};
            copy_region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy_region.srcOffset = {0, 0, 0};
            copy_region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy_region.dstOffset = {0, 0, 0};
            copy_region.extent = {
                (uint32_t)staging_texture.tex_width,
                (uint32_t)staging_texture.tex_height, 1};
            vkCmdCopyImage(
                m_setup_cmd, staging_texture.image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_textures[i].image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

            set_image_layout(m_textures[i].image,
                             VK_IMAGE_ASPECT_COLOR_BIT,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             m_textures[i].imageLayout,
                             0);

            demo_flush_init_cmd(this);

            destroy_texture_image(&staging_texture);
        } else {
            /* Can't support VK_FORMAT_B8G8R8A8_UNORM !? */
            Fl::fatal("No support for B8G8R8A8_UNORM as texture image format");
        }

        VkSamplerCreateInfo sampler = {};
        sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler.pNext = NULL;
        sampler.magFilter = VK_FILTER_NEAREST;
        sampler.minFilter = VK_FILTER_NEAREST;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.mipLodBias = 0.0f;
        sampler.anisotropyEnable = VK_FALSE;
        sampler.maxAnisotropy = 1;
        sampler.compareOp = VK_COMPARE_OP_NEVER;
        sampler.minLod = 0.0f;
        sampler.maxLod = 0.0f;
        sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        sampler.unnormalizedCoordinates = VK_FALSE;
        
        VkImageViewCreateInfo view = {};
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.pNext = NULL;
        view.image = VK_NULL_HANDLE;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = tex_format;
        view.components =
            {
                VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A,
            };
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        view.flags = 0;

        /* create sampler */
        result = vkCreateSampler(m_device, &sampler, NULL,
                              &m_textures[i].sampler);
        VK_CHECK_RESULT(result);

        /* create image view */
        view.image = m_textures[i].image;
        result = vkCreateImageView(m_device, &view, NULL,
                                &m_textures[i].view);
        VK_CHECK_RESULT(result);
    }
}

void vk_shape_window::prepare()
{
    prepare_textures();  // must refactor to window
    // demo_prepare_vertices(this);  // must refactor to window
    // demo_prepare_descriptor_layout(this);  // uses texture count
    // demo_prepare_render_pass(this);  // can be kept in driver
    // demo_prepare_pipeline(this);     // must go into Fl_Vk_Window
}

void vk_shape_window::draw() {
    if (!shown() || w() <= 0 || h() <= 0) return;
    printf("Fl_Vk_Window::draw called\n");

    // Background color
    m_clearColor = { 0.0, 0.0, 1.0, 1.0 };
    
    draw_begin();

    // Draw the triangle
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(m_draw_cmd, VERTEX_BUFFER_BIND_ID, 1,
                           &m_vertices.buf, offsets);

    vkCmdDraw(m_draw_cmd, 3, 1, 0, 0);

    Fl_Window::draw();
    
    draw_end();
}

#else

#include <FL/Fl_Box.H>
class vk_shape_window : public Fl_Box {
public:
  int sides;
  vk_shape_window(int x,int y,int w,int h,const char *l=0)
    :Fl_Box(FL_DOWN_BOX,x,y,w,h,l){
      label("This demo does\nnot work without Vulkan");
  }
};

#endif

// when you change the data, as in this callback, you must call redraw():
void sides_cb(Fl_Widget *o, void *p) {
  vk_shape_window *sw = (vk_shape_window *)p;
  sw->sides = int(((Fl_Slider *)o)->value());
  sw->redraw();
}

int main(int argc, char **argv) {
    Fl::use_high_res_VK(1);

#ifdef _WIN32
    char* term = fl_getenv("TERM");
    if (!term || strlen(term) == 0)
    {
        BOOL ok = AttachConsole(ATTACH_PARENT_PROCESS);
        if (ok)
        {
            freopen("conout$", "w", stdout);
            freopen("conout$", "w", stderr);
        }
    }
#endif
  
#if 1
    Fl_Window window(300, 330);
  
// the shape window could be it's own window, but here we make it
// a child window:
        
    vk_shape_window sw(10, 10, 280, 280);

// // make it resize:
    //  window.size_range(300,330,0,0,1,1,1);
// add a knob to control it:
    Fl_Hor_Slider slider(50, 295, window.w()-60, 30, "Sides:");
    slider.align(FL_ALIGN_LEFT);
    slider.step(1);
    slider.bounds(3,40);

    window.resizable(&sw);
    slider.value(sw.sides);
    slider.callback(sides_cb,&sw);
    window.end();
    window.show(argc,argv);
#else
    vk_shape_window sw(300, 330, "VK Window");
    sw.resizable(&sw);
    sw.show();
#endif
    return Fl::run();
}
