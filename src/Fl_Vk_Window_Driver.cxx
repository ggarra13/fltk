
#include "FL/vk.h"
#include "Fl_Vk_Window_Driver.H"

#ifndef _WIN32
#  include <signal.h>
#endif

#include <cassert>
#include <set>
#include <stdexcept>
#include <vector>


// demo macros
#define DEMO_TEXTURE_COUNT 1
#define VERTEX_BUFFER_BIND_ID 0
#define CLAMP(v, vmin, vmax) (v < vmin ? vmin : (v > vmax ? vmax : v))

struct Fl_Vk_Frame;

bool g_FunctionsLoaded = true;

// Helpers

VKAPI_ATTR VkBool32 VKAPI_CALL
BreakCallback(VkFlags msgFlags, VkDebugReportObjectTypeEXT objType,
              uint64_t srcObject, size_t location, int32_t msgCode,
              const char *pLayerPrefix, const char *pMsg,
              void *pUserData) {
#ifdef _WIN32
    DebugBreak();
#else
    raise(SIGTRAP);
#endif

    return false;
}

static int validation_error = 0;

VKAPI_ATTR VkBool32 VKAPI_CALL
dbgFunc(VkFlags msgFlags, VkDebugReportObjectTypeEXT objType,
    uint64_t srcObject, size_t location, int32_t msgCode,
    const char *pLayerPrefix, const char *pMsg, void *pUserData) {
    char *message = (char *)malloc(strlen(pMsg) + 100);

    assert(message);

    validation_error = 1;

    if (msgFlags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
        sprintf(message, "ERROR: [%s] Code %d : %s", pLayerPrefix, msgCode,
            pMsg);
    } else if (msgFlags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
        sprintf(message, "WARNING: [%s] Code %d : %s", pLayerPrefix, msgCode,
            pMsg);
    } else {
        return false;
    }

    printf("%s\n", message);
    fflush(stdout);
    free(message);

    /*
    * false indicates that layer should not bail-out of an
    * API call that had validation failures. This may mean that the
    * app dies inside the driver due to invalid parameter(s).
    * That's what would happen without validation layers, so we'll
    * keep that behavior here.
    */
    return false;
}

/*
 * Return 1 (true) if all layer names specified in check_names
 * can be found in given layer properties.
 */
static VkBool32 demo_check_layers(uint32_t check_count,
                                  const char **check_names,
                                  uint32_t layer_count,
                                  VkLayerProperties *layers) {
    uint32_t i, j;
    for (i = 0; i < check_count; i++) {
        VkBool32 found = 0;
        for (j = 0; j < layer_count; j++) {
            if (!strcmp(check_names[i], layers[j].layerName)) {
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "Cannot find layer: %s\n", check_names[i]);
            return 0;
        }
    }
    return 1;
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

static void demo_set_image_layout(Fl_Vk_Window* pWindow, VkImage image,
                                  VkImageAspectFlags aspectMask,
                                  VkImageLayout old_image_layout,
                                  VkImageLayout new_image_layout,
                                  int srcAccessMaskInt) {
    VkResult err;

    VkAccessFlagBits srcAccessMask = static_cast<VkAccessFlagBits>(srcAccessMaskInt);
    if (pWindow->m_setup_cmd == VK_NULL_HANDLE) {
        VkCommandBufferAllocateInfo cmd = {};
        cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmd.pNext = NULL;
        cmd.commandPool = pWindow->m_cmd_pool;
        cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmd.commandBufferCount = 1;
        
        err = vkAllocateCommandBuffers(pWindow->m_device, &cmd,
                                       &pWindow->m_setup_cmd);
        assert(!err);

        VkCommandBufferBeginInfo cmd_buf_info = {};
        cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmd_buf_info.pNext = NULL;
        cmd_buf_info.flags = 0;
        cmd_buf_info.pInheritanceInfo = NULL;
        
        err = vkBeginCommandBuffer(pWindow->m_setup_cmd, &cmd_buf_info);
        assert(!err);
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

    vkCmdPipelineBarrier(pWindow->m_setup_cmd,
                         src_stages, dest_stages, 0, 0, NULL,
                         0, NULL, 1, &image_memory_barrier);
}

static void demo_prepare_buffers(Fl_Vk_Window* pWindow) {
    VkResult result;
    VkSwapchainKHR oldSwapchain = pWindow->m_swapchain;
    pWindow->m_swapchain = VK_NULL_HANDLE;

    // Get surface capabilities
    VkSurfaceCapabilitiesKHR surfCapabilities;
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        pWindow->m_gpu, pWindow->m_surface, &surfCapabilities);
    VK_CHECK_RESULT(result);

    // Log surface capabilities for debugging
    printf("Surface capabilities: min=%ux%u, max=%ux%u, current=%ux%u\n",
           surfCapabilities.minImageExtent.width, surfCapabilities.minImageExtent.height,
           surfCapabilities.maxImageExtent.width, surfCapabilities.maxImageExtent.height,
           surfCapabilities.currentExtent.width, surfCapabilities.currentExtent.height);
    printf("Requested size: %ux%u\n", pWindow->w(), pWindow->h());

    // Set swapchain extent to match window size, clamped to capabilities
    VkExtent2D swapchainExtent = {(uint32_t)pWindow->w(),
        (uint32_t)pWindow->h()};
    printf("Wanted swapchain extent: %ux%u\n", swapchainExtent.width, swapchainExtent.height);
    swapchainExtent.width = CLAMP(swapchainExtent.width,
                                  surfCapabilities.minImageExtent.width,
                                  surfCapabilities.maxImageExtent.width);
    swapchainExtent.height = CLAMP(swapchainExtent.height,
                                   surfCapabilities.minImageExtent.height,
                                   surfCapabilities.maxImageExtent.height);

    // Log the final swapchain extent
    printf("Final swapchain extent: %ux%u\n", swapchainExtent.width, swapchainExtent.height);

    // Check if extent matches request; if not, delay recreation
    if (swapchainExtent.width != pWindow->w() || swapchainExtent.height != pWindow->h())
    {
        printf("Surface not ready for requested size; deferring swapchain recreation\n");
        pWindow->m_swapchain = oldSwapchain; // Restore old swapchain
        return;
    }
    
    VkSwapchainCreateInfoKHR swapchain = {};
    swapchain.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain.surface = pWindow->m_surface;
    swapchain.minImageCount = 2;  // Ensure double bvuffering
    swapchain.imageFormat = pWindow->m_format;
    swapchain.imageColorSpace = pWindow->m_color_space;
    swapchain.imageExtent = swapchainExtent;
    swapchain.imageArrayLayers = 1;
    swapchain.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain.preTransform = surfCapabilities.currentTransform;
    swapchain.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain.oldSwapchain = oldSwapchain;
    swapchain.clipped = VK_TRUE;

    // Destroy old buffers
    if (pWindow->m_buffers) {
        for (uint32_t i = 0; i < pWindow->m_swapchainImageCount; i++) {
            vkDestroyImageView(pWindow->m_device, pWindow->m_buffers[i].view, NULL);
        }
        free(pWindow->m_buffers);
        pWindow->m_buffers = NULL;
    }

    // Create new swapchain
    result = vkCreateSwapchainKHR(pWindow->m_device, &swapchain, NULL, &pWindow->m_swapchain);
    VK_CHECK_RESULT(result);
    if (pWindow->m_swapchain == VK_NULL_HANDLE)
    {
        printf("swapchain creation failed and returned NULL HANDLE\n");
    }

    if (oldSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(pWindow->m_device, oldSwapchain, NULL);
    }

    // Get new swapchain images
    result = vkGetSwapchainImagesKHR(pWindow->m_device, pWindow->m_swapchain,
                                     &pWindow->m_swapchainImageCount, NULL);
    VK_CHECK_RESULT(result);
    VkImage *swapchainImages = (VkImage *)malloc(pWindow->m_swapchainImageCount * sizeof(VkImage));
    assert(swapchainImages);
    result = vkGetSwapchainImagesKHR(pWindow->m_device, pWindow->m_swapchain,
                                     &pWindow->m_swapchainImageCount, swapchainImages);
    VK_CHECK_RESULT(result);

    // Recreate buffers
    pWindow->m_buffers = (Fl_Vk_SwapchainBuffer *)malloc(pWindow->m_swapchainImageCount * sizeof(Fl_Vk_SwapchainBuffer));
    assert(pWindow->m_buffers);
    for (uint32_t i = 0; i < pWindow->m_swapchainImageCount; i++) {
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = swapchainImages[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = pWindow->m_format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.layerCount = 1;
        result = vkCreateImageView(pWindow->m_device, &view_info, NULL, &pWindow->m_buffers[i].view);
        VK_CHECK_RESULT(result);
        pWindow->m_buffers[i].image = swapchainImages[i];
    }

    free(swapchainImages);
}

static void demo_prepare_depth(Fl_Vk_Window* pWindow) {
    
    const VkFormat depth_format = VK_FORMAT_D16_UNORM;
    VkImageCreateInfo image = {};
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.pNext = NULL;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = depth_format;
    image.extent = {(uint32_t)pWindow->w(), (uint32_t)pWindow->h(), 1};
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image.flags = 0;
    
    VkMemoryAllocateInfo mem_alloc = {};
    mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_alloc.pNext = NULL;
    mem_alloc.allocationSize = 0;
    mem_alloc.memoryTypeIndex = 0;

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;
                      

    VkImageViewCreateInfo view = {};
    view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.pNext = NULL;
    view.image = VK_NULL_HANDLE;
    view.format = depth_format;
    view.subresourceRange = subresourceRange;
    view.flags = 0;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;

    VkMemoryRequirements mem_reqs;
    VkResult result;
    bool pass;

    pWindow->m_depth.format = depth_format;

    /* create image */
    result = vkCreateImage(pWindow->m_device, &image, NULL, &pWindow->m_depth.image);
    VK_CHECK_RESULT(result);

    /* get memory requirements for this object */
    vkGetImageMemoryRequirements(pWindow->m_device, pWindow->m_depth.image, &mem_reqs);

    /* select memory size and type */
    mem_alloc.allocationSize = mem_reqs.size;
    pass = memory_type_from_properties(pWindow, mem_reqs.memoryTypeBits,
                                       0, /* No requirements */
                                       &mem_alloc.memoryTypeIndex);
    assert(pass);

    /* allocate memory */
    result = vkAllocateMemory(pWindow->m_device, &mem_alloc, NULL, &pWindow->m_depth.mem);
    VK_CHECK_RESULT(result);

    /* bind memory */
    result =
        vkBindImageMemory(pWindow->m_device, pWindow->m_depth.image, pWindow->m_depth.mem, 0);
    VK_CHECK_RESULT(result);

    demo_set_image_layout(pWindow, pWindow->m_depth.image,
                          VK_IMAGE_ASPECT_DEPTH_BIT,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                          0);

    /* create image view */
    view.image = pWindow->m_depth.image;
    result = vkCreateImageView(pWindow->m_device, &view, NULL, &pWindow->m_depth.view);
    VK_CHECK_RESULT(result);
}

static void
demo_prepare_texture_image(Fl_Vk_Window* pWindow, const uint32_t *tex_colors,
                           Fl_Vk_Texture* tex_obj, VkImageTiling tiling,
                           VkImageUsageFlags usage, VkFlags required_props) {
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
        vkCreateImage(pWindow->m_device, &image_create_info, NULL, &tex_obj->image);
    VK_CHECK_RESULT(result);

    vkGetImageMemoryRequirements(pWindow->m_device, tex_obj->image, &mem_reqs);

    mem_alloc.allocationSize = mem_reqs.size;
    pass =
        memory_type_from_properties(pWindow, mem_reqs.memoryTypeBits,
                                    required_props, &mem_alloc.memoryTypeIndex);
    assert(pass);

    /* allocate memory */
    result = vkAllocateMemory(pWindow->m_device, &mem_alloc, NULL, &tex_obj->mem);
    VK_CHECK_RESULT(result);

    /* bind memory */
    result = vkBindImageMemory(pWindow->m_device, tex_obj->image, tex_obj->mem, 0);
    VK_CHECK_RESULT(result);

    if (required_props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        VkImageSubresource subres = {};
        subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subres.mipLevel = 0;
        subres.arrayLayer = 0;
        VkSubresourceLayout layout;
        void *data;
        int32_t x, y;

        vkGetImageSubresourceLayout(pWindow->m_device, tex_obj->image, &subres,
                                    &layout);

        result = vkMapMemory(pWindow->m_device, tex_obj->mem, 0,
                             mem_alloc.allocationSize, 0, &data);
        VK_CHECK_RESULT(result);

        for (y = 0; y < tex_height; y++) {
            uint32_t *row = (uint32_t *)((char *)data + layout.rowPitch * y);
            for (x = 0; x < tex_width; x++)
                row[x] = tex_colors[(x & 1) ^ (y & 1)];
        }

        vkUnmapMemory(pWindow->m_device, tex_obj->mem);
    }

    tex_obj->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    demo_set_image_layout(pWindow, tex_obj->image, VK_IMAGE_ASPECT_COLOR_BIT,
                          VK_IMAGE_LAYOUT_PREINITIALIZED, tex_obj->imageLayout,
                          VK_ACCESS_HOST_WRITE_BIT);
    /* setting the image layout does not reference the actual memory so no need
     * to add a mem ref */
}

static void demo_destroy_texture_image(Fl_Vk_Window *pWindow,
                                       Fl_Vk_Texture *tex_obj) {
    // /* clean up staging resources */
    vkDestroyImage(pWindow->m_device, tex_obj->image, NULL);
    vkFreeMemory(pWindow->m_device, tex_obj->mem, NULL);
}

static void demo_prepare_textures(Fl_Vk_Window *pWindow) {
    const VkFormat tex_format = VK_FORMAT_B8G8R8A8_UNORM;
    VkFormatProperties props;
    const uint32_t tex_colors[DEMO_TEXTURE_COUNT][2] = {
        {0xffff0000, 0xff00ff00},
    };
    uint32_t i;
    VkResult result;

    vkGetPhysicalDeviceFormatProperties(pWindow->m_gpu, tex_format, &props);

    for (i = 0; i < DEMO_TEXTURE_COUNT; i++) {
        if ((props.linearTilingFeatures &
             VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) &&
            !pWindow->m_use_staging_buffer) {
            /* Device can texture using linear textures */
            demo_prepare_texture_image(
                pWindow, tex_colors[i], &pWindow->m_textures[i], VK_IMAGE_TILING_LINEAR,
                VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        } else if (props.optimalTilingFeatures &
                   VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            /* Must use staging buffer to copy linear texture to optimized */
            Fl_Vk_Texture staging_texture;

            memset(&staging_texture, 0, sizeof(staging_texture));
            demo_prepare_texture_image(
                pWindow, tex_colors[i], &staging_texture,
                VK_IMAGE_TILING_LINEAR,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            demo_prepare_texture_image(
                pWindow, tex_colors[i], &pWindow->m_textures[i],
                VK_IMAGE_TILING_OPTIMAL,
                (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            demo_set_image_layout(pWindow, staging_texture.image,
                                  VK_IMAGE_ASPECT_COLOR_BIT,
                                  staging_texture.imageLayout,
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                  0);

            demo_set_image_layout(pWindow, pWindow->m_textures[i].image,
                                  VK_IMAGE_ASPECT_COLOR_BIT,
                                  pWindow->m_textures[i].imageLayout,
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
                pWindow->m_setup_cmd, staging_texture.image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, pWindow->m_textures[i].image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

            demo_set_image_layout(pWindow, pWindow->m_textures[i].image,
                                  VK_IMAGE_ASPECT_COLOR_BIT,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  pWindow->m_textures[i].imageLayout,
                                  0);

            demo_flush_init_cmd(pWindow);

            demo_destroy_texture_image(pWindow, &staging_texture);
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
        result = vkCreateSampler(pWindow->m_device, &sampler, NULL,
                              &pWindow->m_textures[i].sampler);
        VK_CHECK_RESULT(result);

        /* create image view */
        view.image = pWindow->m_textures[i].image;
        result = vkCreateImageView(pWindow->m_device, &view, NULL,
                                &pWindow->m_textures[i].view);
        VK_CHECK_RESULT(result);
    }
}

static void demo_prepare_vertices(Fl_Vk_Window* pWindow) {
    // clang-format off
    const float vb[3][5] = {
        /*      position             texcoord */
        { -1.0f, -1.0f,  0.25f,     0.0f, 0.0f },
        {  1.0f, -1.0f,  0.25f,     1.0f, 0.0f },
        {  0.0f,  1.0f,  1.0f,      0.5f, 1.0f },
    };
    // clang-format on
    VkBufferCreateInfo buf_info = {};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.pNext = NULL;
    buf_info.size = sizeof(vb);
    buf_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buf_info.flags = 0;
    
    VkMemoryAllocateInfo mem_alloc = {};
    mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_alloc.pNext = NULL;
    mem_alloc.allocationSize = 0;
    mem_alloc.memoryTypeIndex = 0;
    
    VkMemoryRequirements mem_reqs;
    VkResult result;
    bool pass;
    void *data;

    memset(&pWindow->m_vertices, 0, sizeof(pWindow->m_vertices));

    result = vkCreateBuffer(pWindow->m_device, &buf_info, NULL, &pWindow->m_vertices.buf);
    VK_CHECK_RESULT(result);

    vkGetBufferMemoryRequirements(pWindow->m_device, pWindow->m_vertices.buf, &mem_reqs);
    VK_CHECK_RESULT(result);

    mem_alloc.allocationSize = mem_reqs.size;
    pass = memory_type_from_properties(pWindow, mem_reqs.memoryTypeBits,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       &mem_alloc.memoryTypeIndex);
    assert(pass);

    result = vkAllocateMemory(pWindow->m_device, &mem_alloc, NULL, &pWindow->m_vertices.mem);
    VK_CHECK_RESULT(result);

    result = vkMapMemory(pWindow->m_device, pWindow->m_vertices.mem, 0,
                      mem_alloc.allocationSize, 0, &data);
    VK_CHECK_RESULT(result);

    memcpy(data, vb, sizeof(vb));

    vkUnmapMemory(pWindow->m_device, pWindow->m_vertices.mem);

    result = vkBindBufferMemory(pWindow->m_device, pWindow->m_vertices.buf,
                             pWindow->m_vertices.mem, 0);
    VK_CHECK_RESULT(result);

    pWindow->m_vertices.vi.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pWindow->m_vertices.vi.pNext = NULL;
    pWindow->m_vertices.vi.vertexBindingDescriptionCount = 1;
    pWindow->m_vertices.vi.pVertexBindingDescriptions = pWindow->m_vertices.vi_bindings;
    pWindow->m_vertices.vi.vertexAttributeDescriptionCount = 2;
    pWindow->m_vertices.vi.pVertexAttributeDescriptions = pWindow->m_vertices.vi_attrs;

    pWindow->m_vertices.vi_bindings[0].binding = VERTEX_BUFFER_BIND_ID;
    pWindow->m_vertices.vi_bindings[0].stride = sizeof(vb[0]);
    pWindow->m_vertices.vi_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    pWindow->m_vertices.vi_attrs[0].binding = VERTEX_BUFFER_BIND_ID;
    pWindow->m_vertices.vi_attrs[0].location = 0;
    pWindow->m_vertices.vi_attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    pWindow->m_vertices.vi_attrs[0].offset = 0;

    pWindow->m_vertices.vi_attrs[1].binding = VERTEX_BUFFER_BIND_ID;
    pWindow->m_vertices.vi_attrs[1].location = 1;
    pWindow->m_vertices.vi_attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    pWindow->m_vertices.vi_attrs[1].offset = sizeof(float) * 3;
}

static void demo_prepare_descriptor_layout(Fl_Vk_Window* pWindow) {
    VkDescriptorSetLayoutBinding layout_binding = {};
    layout_binding.binding = 0;
    layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layout_binding.descriptorCount = DEMO_TEXTURE_COUNT;
    layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layout_binding.pImmutableSamplers = NULL;
  
    VkDescriptorSetLayoutCreateInfo descriptor_layout = {};
    descriptor_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_layout.pNext = NULL;
    descriptor_layout.bindingCount = 1;
    descriptor_layout.pBindings = &layout_binding;
                 
    VkResult result;

    result = vkCreateDescriptorSetLayout(pWindow->m_device, &descriptor_layout, NULL,
                                      &pWindow->m_desc_layout);
    VK_CHECK_RESULT(result);

    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
    pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pPipelineLayoutCreateInfo.pNext = NULL;
    pPipelineLayoutCreateInfo.setLayoutCount = 1;
    pPipelineLayoutCreateInfo.pSetLayouts = &pWindow->m_desc_layout;

    result = vkCreatePipelineLayout(pWindow->m_device, &pPipelineLayoutCreateInfo, NULL,
                                 &pWindow->m_pipeline_layout);
    VK_CHECK_RESULT(result);
}

static void demo_prepare_render_pass(Fl_Vk_Window* pWindow) {
    VkAttachmentDescription attachments[2];
    attachments[0] = VkAttachmentDescription();
    attachments[0].format = pWindow->m_format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                 
    attachments[1] = VkAttachmentDescription();
    attachments[1].format = pWindow->m_depth.format;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                
    
    VkAttachmentReference color_reference = {};
    color_reference.attachment = 0;
    color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkAttachmentReference depth_reference = {};
    depth_reference.attachment = 1;
    depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.flags = 0;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = NULL;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_reference;
    subpass.pResolveAttachments = NULL;
    subpass.pDepthStencilAttachment = &depth_reference;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = NULL;
                             
    VkRenderPassCreateInfo rp_info = {};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.pNext = NULL;
    rp_info.attachmentCount = 2;
    rp_info.pAttachments = attachments;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    rp_info.dependencyCount = 0;
    rp_info.pDependencies = NULL;
                    
    VkResult result;
    result = vkCreateRenderPass(pWindow->m_device, &rp_info, NULL, &pWindow->m_renderPass);
    VK_CHECK_RESULT(result);
}

static VkShaderModule
demo_prepare_shader_module(Fl_Vk_Window* pWindow, const uint32_t *code, size_t size) {
    VkShaderModuleCreateInfo moduleCreateInfo;
    VkShaderModule module;
    VkResult result;

    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.pNext = NULL;

    moduleCreateInfo.codeSize = size;
    moduleCreateInfo.pCode = code;
    moduleCreateInfo.flags = 0;
    result = vkCreateShaderModule(pWindow->m_device, &moduleCreateInfo, NULL, &module);
    VK_CHECK_RESULT(result);

    return module;
}

static VkShaderModule demo_prepare_vs(Fl_Vk_Window* pWindow) {
    size_t size = sizeof(vertShaderCode);

    pWindow->m_vert_shader_module =
        demo_prepare_shader_module(pWindow, vertShaderCode, size);

    return pWindow->m_vert_shader_module;
}

static VkShaderModule demo_prepare_fs(Fl_Vk_Window* pWindow) {
    size_t size = sizeof(fragShaderCode);

    pWindow->m_frag_shader_module =
        demo_prepare_shader_module(pWindow, fragShaderCode, size);

    return pWindow->m_frag_shader_module;
}

static void demo_prepare_pipeline(Fl_Vk_Window* pWindow) {
    VkGraphicsPipelineCreateInfo pipeline;
    VkPipelineCacheCreateInfo pipelineCache;

    VkPipelineVertexInputStateCreateInfo vi;
    VkPipelineInputAssemblyStateCreateInfo ia;
    VkPipelineRasterizationStateCreateInfo rs;
    VkPipelineColorBlendStateCreateInfo cb;
    VkPipelineDepthStencilStateCreateInfo ds;
    VkPipelineViewportStateCreateInfo vp;
    VkPipelineMultisampleStateCreateInfo ms;
    VkDynamicState dynamicStateEnables[(VK_DYNAMIC_STATE_STENCIL_REFERENCE - VK_DYNAMIC_STATE_VIEWPORT + 1)];
    VkPipelineDynamicStateCreateInfo dynamicState;

    VkResult result;

    memset(dynamicStateEnables, 0, sizeof dynamicStateEnables);
    memset(&dynamicState, 0, sizeof dynamicState);
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pDynamicStates = dynamicStateEnables;

    memset(&pipeline, 0, sizeof(pipeline));
    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.layout = pWindow->m_pipeline_layout;

    vi = pWindow->m_vertices.vi;

    memset(&ia, 0, sizeof(ia));
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    memset(&rs, 0, sizeof(rs));
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rs.depthClampEnable = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.depthBiasEnable = VK_FALSE;
    rs.lineWidth = 1.0f;

    memset(&cb, 0, sizeof(cb));
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    VkPipelineColorBlendAttachmentState att_state[1];
    memset(att_state, 0, sizeof(att_state));
    att_state[0].colorWriteMask = 0xf;
    att_state[0].blendEnable = VK_FALSE;
    cb.attachmentCount = 1;
    cb.pAttachments = att_state;

    memset(&vp, 0, sizeof(vp));
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    dynamicStateEnables[dynamicState.dynamicStateCount++] =
        VK_DYNAMIC_STATE_VIEWPORT;
    vp.scissorCount = 1;
    dynamicStateEnables[dynamicState.dynamicStateCount++] =
        VK_DYNAMIC_STATE_SCISSOR;

    memset(&ds, 0, sizeof(ds));
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.back.failOp = VK_STENCIL_OP_KEEP;
    ds.back.passOp = VK_STENCIL_OP_KEEP;
    ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
    ds.stencilTestEnable = VK_FALSE;
    ds.front = ds.back;

    memset(&ms, 0, sizeof(ms));
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.pSampleMask = NULL;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Two stages: vs and fs
    pipeline.stageCount = 2;
    VkPipelineShaderStageCreateInfo shaderStages[2];
    memset(&shaderStages, 0, 2 * sizeof(VkPipelineShaderStageCreateInfo));

    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = demo_prepare_vs(pWindow);
    shaderStages[0].pName = "main";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = demo_prepare_fs(pWindow);
    shaderStages[1].pName = "main";

    pipeline.pVertexInputState = &vi;
    pipeline.pInputAssemblyState = &ia;
    pipeline.pRasterizationState = &rs;
    pipeline.pColorBlendState = &cb;
    pipeline.pMultisampleState = &ms;
    pipeline.pViewportState = &vp;
    pipeline.pDepthStencilState = &ds;
    pipeline.pStages = shaderStages;
    pipeline.renderPass = pWindow->m_renderPass;
    pipeline.pDynamicState = &dynamicState;

    memset(&pipelineCache, 0, sizeof(pipelineCache));
    pipelineCache.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    result = vkCreatePipelineCache(pWindow->m_device, &pipelineCache, NULL,
                                &pWindow->m_pipelineCache);
    VK_CHECK_RESULT(result);
    result = vkCreateGraphicsPipelines(pWindow->m_device, pWindow->m_pipelineCache, 1,
                                    &pipeline, NULL, &pWindow->m_pipeline);
    VK_CHECK_RESULT(result);

    vkDestroyPipelineCache(pWindow->m_device, pWindow->m_pipelineCache, NULL);

    vkDestroyShaderModule(pWindow->m_device, pWindow->m_frag_shader_module, NULL);
    vkDestroyShaderModule(pWindow->m_device, pWindow->m_vert_shader_module, NULL);
}

static void demo_prepare_descriptor_pool(Fl_Vk_Window* pWindow) {
    VkDescriptorPoolSize type_count = {};
    type_count.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    type_count.descriptorCount = DEMO_TEXTURE_COUNT;
    
    VkDescriptorPoolCreateInfo descriptor_pool = {};
    descriptor_pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool.pNext = NULL;
    descriptor_pool.maxSets = 1;
    descriptor_pool.poolSizeCount = 1;
    descriptor_pool.pPoolSizes = &type_count;

    VkResult result;
             
    result = vkCreateDescriptorPool(pWindow->m_device, &descriptor_pool, NULL,
                                 &pWindow->m_desc_pool);
    VK_CHECK_RESULT(result);
}

static void demo_prepare_descriptor_set(Fl_Vk_Window* pWindow) {
    VkDescriptorImageInfo tex_descs[DEMO_TEXTURE_COUNT];
    VkResult result;
    uint32_t i;

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pNext = NULL;
    alloc_info.descriptorPool = pWindow->m_desc_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &pWindow->m_desc_layout;
        
    result = vkAllocateDescriptorSets(pWindow->m_device, &alloc_info,
                                      &pWindow->m_desc_set);
    VK_CHECK_RESULT(result);

    memset(&tex_descs, 0, sizeof(tex_descs));
    for (i = 0; i < DEMO_TEXTURE_COUNT; i++) {
        tex_descs[i].sampler = pWindow->m_textures[i].sampler;
        tex_descs[i].imageView = pWindow->m_textures[i].view;
        tex_descs[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = pWindow->m_desc_set;
    write.descriptorCount = DEMO_TEXTURE_COUNT;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = tex_descs;

    vkUpdateDescriptorSets(pWindow->m_device, 1, &write, 0, NULL);
}

static void demo_prepare_framebuffers(Fl_Vk_Window* pWindow) {
    VkImageView attachments[2];
    attachments[1] = pWindow->m_depth.view;

    VkFramebufferCreateInfo fb_info = {};
    fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.pNext = NULL;
    fb_info.renderPass = pWindow->m_renderPass;
    fb_info.attachmentCount = 2;
    fb_info.pAttachments = attachments;
    fb_info.width = pWindow->w();
    fb_info.height = pWindow->h();
    fb_info.layers = 1;
    
    VkResult result;
    uint32_t i;

    pWindow->m_framebuffers = (VkFramebuffer *)VK_ALLOC(pWindow->m_swapchainImageCount *
                                                 sizeof(VkFramebuffer));
    assert(pWindow->m_framebuffers);

    for (i = 0; i < pWindow->m_swapchainImageCount; i++) {
        attachments[0] = pWindow->m_buffers[i].view;
        result = vkCreateFramebuffer(pWindow->m_device, &fb_info, NULL,
                                  &pWindow->m_framebuffers[i]);
        VK_CHECK_RESULT(result);
    }
}


bool
Fl_Vk_Window_Driver::check_device_extension_support(VkPhysicalDevice device)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                         availableExtensions.data());

    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    std::set<std::string> requiredExtensions(deviceExtensions.begin(),
                                             deviceExtensions.end());

    for (const auto& extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

QueueFamilyIndices
Fl_Vk_Window_Driver::find_queue_families(VkPhysicalDevice physicalDevice)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice,
                                             &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice,
                                             &queueFamilyCount,
                                             queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i,
                                             pWindow->m_surface,
                                             &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}


static void demo_init_device(Fl_Vk_Window* pWindow) {
    VkResult result;

    float queue_priorities[1] = {0.0};
    VkDeviceQueueCreateInfo queue = {};                                 
    queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.pNext = NULL;
    queue.queueFamilyIndex = pWindow->m_graphics_queue_node_index;
    queue.queueCount = 1;
    queue.pQueuePriorities = queue_priorities;
    
    VkDeviceCreateInfo device = {};
    device.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device.pNext = NULL;
    device.queueCreateInfoCount = 1;
    device.pQueueCreateInfos = &queue;
    device.enabledLayerCount = 0;
    device.ppEnabledLayerNames = NULL;
    device.enabledExtensionCount = pWindow->m_enabled_extension_count;
    device.ppEnabledExtensionNames = (const char *const *)pWindow->m_extension_names;

    result = vkCreateDevice(pWindow->m_gpu, &device, NULL, &pWindow->m_device);
    VK_CHECK_RESULT(result);
}

static void demo_init_vk(Fl_Vk_Window* pWindow)
{
    VkResult err;
    VkBool32 portability_enumeration = VK_FALSE;
    uint32_t i = 0;
    uint32_t required_extension_count = 0;
    uint32_t instance_extension_count = 0;
    uint32_t instance_layer_count = 0;
    uint32_t validation_layer_count = 0;
    const char **instance_validation_layers = NULL;
    pWindow->m_enabled_extension_count = 0;
    pWindow->m_enabled_layer_count = 0;

    const char *instance_validation_layers_alt1[] = {
        //"VK_LAYER_LUNARG_standard_validation",  // Not found on windows
#if defined(_WIN32) || defined(__APPLE__)
        "VK_LAYER_KHRONOS_validation",  // Not found on X11
#endif
    };

    const char *instance_validation_layers_alt2[] = {
        //"VK_LAYER_GOOGLE_threading",  // Not found on windows
#ifdef _WIN32
        "VK_LAYER_LUNARG_parameter_validation",  // windows only
#endif
        "VK_LAYER_LUNARG_object_tracker",  "VK_LAYER_LUNARG_image",
        "VK_LAYER_LUNARG_core_validation", "VK_LAYER_LUNARG_swapchain",
        "VK_LAYER_GOOGLE_unique_objects"
    };

    /* Look for validation layers */
    VkBool32 validation_found = 0;
    if (pWindow->m_validate) {

        err = vkEnumerateInstanceLayerProperties(&instance_layer_count, NULL);
        assert(!err);

        instance_validation_layers = (const char**) instance_validation_layers_alt1;
        if (instance_layer_count > 0) {
            VkLayerProperties *instance_layers = (VkLayerProperties*)
                    VK_ALLOC(sizeof (VkLayerProperties) * instance_layer_count);
            err = vkEnumerateInstanceLayerProperties(&instance_layer_count,
                    instance_layers);
            assert(!err);


            validation_found = demo_check_layers(
                    VK_ARRAY_SIZE(instance_validation_layers_alt1),
                    instance_validation_layers, instance_layer_count,
                    instance_layers);
            if (validation_found) {
                pWindow->m_enabled_layer_count = VK_ARRAY_SIZE(instance_validation_layers_alt1);
                pWindow->m_enabled_layers[0] = "VK_LAYER_LUNARG_standard_validation";
                validation_layer_count = 1;
            } else {
                // use alternative set of validation layers
                instance_validation_layers =
                    (const char**) instance_validation_layers_alt2;
                pWindow->m_enabled_layer_count = VK_ARRAY_SIZE(instance_validation_layers_alt2);
                validation_found = demo_check_layers(
                    VK_ARRAY_SIZE(instance_validation_layers_alt2),
                    instance_validation_layers, instance_layer_count,
                    instance_layers);
                validation_layer_count =
                    VK_ARRAY_SIZE(instance_validation_layers_alt2);
                for (i = 0; i < validation_layer_count; i++) {
                    pWindow->m_enabled_layers[i] = instance_validation_layers[i];
                }
            }
            free(instance_layers);
        }

        if (!validation_found) {
            Fl::fatal("vkEnumerateInstanceLayerProperties failed to find "
                    "required validation layer.\n\n"
                    "Please look at the Getting Started guide for additional "
                    "information.\n",
                    "vkCreateInstance Failure");
        }
    }

    /* Look for instance extensions */
    auto required_extensions = Fl_Vk_Window_Driver::driver(pWindow)->get_required_extensions();
    if (required_extensions.empty()) {
        Fl::fatal("FLTK get_required_extensions failed to find the "
                 "platform surface extensions.\n\nDo you have a compatible "
                 "Vulkan installable client driver (ICD) installed?\nPlease "
                 "look at the Getting Started guide for additional "
                 "information.\n",
                 "vkCreateInstance Failure");
    }

    required_extension_count = required_extensions.size();
    for (i = 0; i < required_extension_count; i++) {
        pWindow->m_extension_names[pWindow->m_enabled_extension_count++] =
            required_extensions[i].c_str();
        assert(pWindow->m_enabled_extension_count < 64);
    }

    err = vkEnumerateInstanceExtensionProperties(
        NULL, &instance_extension_count, NULL);
    assert(!err);

    if (instance_extension_count > 0) {
        VkExtensionProperties *instance_extensions = (VkExtensionProperties*)
            VK_ALLOC(sizeof(VkExtensionProperties) * instance_extension_count);
        err = vkEnumerateInstanceExtensionProperties(
            NULL, &instance_extension_count, instance_extensions);
        assert(!err);
        for (i = 0; i < instance_extension_count; i++) {
            if (!strcmp(VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
                        instance_extensions[i].extensionName)) {
                if (pWindow->m_validate) {
                    pWindow->m_extension_names[pWindow->m_enabled_extension_count++] =
                        VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
                }
            }
            assert(pWindow->m_enabled_extension_count < 64);
            if (!strcmp(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
                        instance_extensions[i].extensionName)) {
                pWindow->m_extension_names[pWindow->m_enabled_extension_count++] =
                    VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
                portability_enumeration = VK_TRUE;
            }
            assert(pWindow->m_enabled_extension_count < 64);
        }

        free(instance_extensions);
    }

    VkApplicationInfo app = {};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pNext = NULL;
    app.pApplicationName = "FLTK";
    app.applicationVersion = 0;
    app.pEngineName = "FLTK";
    app.engineVersion = 0;
    app.apiVersion = VK_API_VERSION_1_0;
    
    VkInstanceCreateInfo inst_info = {};
    inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_info.pNext = NULL;
    inst_info.pApplicationInfo = &app;
    inst_info.enabledLayerCount = pWindow->m_enabled_layer_count;
    inst_info.ppEnabledLayerNames = (const char *const *)instance_validation_layers;
    inst_info.enabledExtensionCount = pWindow->m_enabled_extension_count;
    inst_info.ppEnabledExtensionNames = (const char *const *)pWindow->m_extension_names;

    if (portability_enumeration)
        inst_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    uint32_t gpu_count;

    err = vkCreateInstance(&inst_info, NULL, &pWindow->m_instance);
    if (err == VK_ERROR_INCOMPATIBLE_DRIVER) {
        Fl::fatal("Cannot find a compatible Vulkan installable client driver "
                 "(ICD).\n\nPlease look at the Getting Started guide for "
                 "additional information.\n",
                 "vkCreateInstance Failure");
    } else if (err == VK_ERROR_EXTENSION_NOT_PRESENT) {
        Fl::fatal("Cannot find a specified extension library"
                 ".\nMake sure your layers path is set appropriately\n",
                 "vkCreateInstance Failure");
    } else if (err) {
        Fl::fatal("vkCreateInstance failed.\n\nDo you have a compatible Vulkan "
                 "installable client driver (ICD) installed?\nPlease look at "
                 "the Getting Started guide for additional information.\n",
                 "vkCreateInstance Failure");
    }

    //gladLoadVulkanUserPtr(NULL, (GLADuserptrloadfunc) glfwGetInstanceProcAddress, pWindow->m_instance);

    // Make initial call to query gpu_count, then second call for gpu info
    err = vkEnumeratePhysicalDevices(pWindow->m_instance, &gpu_count, NULL);
    assert(!err && gpu_count > 0);

    if (gpu_count > 0) {
        VkPhysicalDevice *physical_devices = (VkPhysicalDevice*)
            VK_ALLOC(sizeof(VkPhysicalDevice) * gpu_count);
        err = vkEnumeratePhysicalDevices(pWindow->m_instance, &gpu_count,
                                         physical_devices);
        assert(!err);
        // For tri demo we just grab the first physical device
        pWindow->m_gpu = physical_devices[0];
        free(physical_devices);
    } else {
        Fl::fatal("vkEnumeratePhysicalDevices reported zero accessible devices."
                 "\n\nDo you have a compatible Vulkan installable client"
                 " driver (ICD) installed?\nPlease look at the Getting Started"
                 " guide for additional information.\n",
                 "vkEnumeratePhysicalDevices Failure");
    }

    // gladLoadVulkanUserPtr(pWindow->m_gpu, (GLADuserptrloadfunc) glfwGetInstanceProcAddress, pWindow->m_instance);

    // Look for device extensions
    uint32_t device_extension_count = 0;
    VkBool32 swapchainExtFound = 0;
    pWindow->m_enabled_extension_count = 0;

    err = vkEnumerateDeviceExtensionProperties(pWindow->m_gpu, NULL,
                                               &device_extension_count, NULL);
    assert(!err);

    if (device_extension_count > 0) {
        VkExtensionProperties *device_extensions = (VkExtensionProperties*)
                VK_ALLOC(sizeof(VkExtensionProperties) * device_extension_count);
        err = vkEnumerateDeviceExtensionProperties(
            pWindow->m_gpu, NULL, &device_extension_count, device_extensions);
        assert(!err);

        for (i = 0; i < device_extension_count; i++) {
            if (!strcmp(VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                        device_extensions[i].extensionName)) {
                swapchainExtFound = 1;
                pWindow->m_extension_names[pWindow->m_enabled_extension_count++] =
                    VK_KHR_SWAPCHAIN_EXTENSION_NAME;
            }
            assert(pWindow->m_enabled_extension_count < 64);
        }

        free(device_extensions);
    }

    if (!swapchainExtFound) {
        Fl::fatal("vkEnumerateDeviceExtensionProperties failed to find "
                 "the " VK_KHR_SWAPCHAIN_EXTENSION_NAME
                 " extension.\n\nDo you have a compatible "
                 "Vulkan installable client driver (ICD) installed?\nPlease "
                 "look at the Getting Started guide for additional "
                 "information.\n",
                 "vkCreateInstance Failure");
    }

    if (pWindow->m_validate) {
        // VkDebugReportCallbackCreateInfoEXT dbgCreateInfo;
        // dbgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
        // dbgCreateInfo.flags =
        //     VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
        // dbgCreateInfo.pfnCallback = pWindow->m_use_break ? BreakCallback : dbgFunc;
        // dbgCreateInfo.pUserData = demo;
        // dbgCreateInfo.pNext = NULL;
        // err = vkCreateDebugReportCallbackEXT(pWindow->m_instance, &dbgCreateInfo, NULL,
        //                                      &pWindow->m_msg_callback);
        // switch (err) {
        // case VK_SUCCESS:
        //     break;
        // case VK_ERROR_OUT_OF_HOST_MEMORY:
        //     Fl::fatal("CreateDebugReportCallback: out of host memory\n",
        //              "CreateDebugReportCallback Failure");
        //     break;
        // default:
        //     Fl::fatal("CreateDebugReportCallback: unknown failure\n",
        //              "CreateDebugReportCallback Failure");
        //     break;
        // }
    }

    vkGetPhysicalDeviceProperties(pWindow->m_gpu, &pWindow->m_gpu_props);

    // Query with NULL data to get count
    vkGetPhysicalDeviceQueueFamilyProperties(pWindow->m_gpu, &pWindow->m_queue_count,
                                             NULL);

    pWindow->m_queue_props = (VkQueueFamilyProperties *)VK_ALLOC(
        pWindow->m_queue_count * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(pWindow->m_gpu, &pWindow->m_queue_count,
                                             pWindow->m_queue_props);
    assert(pWindow->m_queue_count >= 1);

    vkGetPhysicalDeviceFeatures(pWindow->m_gpu, &pWindow->m_gpu_features);

    // Graphics queue and MemMgr queue can be separate.
    // TODO: Add support for separate queues, including synchronization,
    //       and appropriate tracking for QueueSubmit
}

static void demo_init_vk_swapchain(Fl_Vk_Window* pWindow)
{
    VkResult result;
    uint32_t i;
    
    // Iterate over each queue to learn whether it supports presenting:
    VkBool32 *supportsPresent =
        (VkBool32 *)VK_ALLOC(pWindow->m_queue_count * sizeof(VkBool32));
    for (i = 0; i < pWindow->m_queue_count; i++) {
        vkGetPhysicalDeviceSurfaceSupportKHR(pWindow->m_gpu, i, pWindow->m_surface,
                                             &supportsPresent[i]);
    }

    // Search for a graphics and a present queue in the array of queue
    // families, try to find one that supports both
    uint32_t graphicsQueueNodeIndex = UINT32_MAX;
    uint32_t presentQueueNodeIndex = UINT32_MAX;
    for (i = 0; i < pWindow->m_queue_count; i++) {
        if ((pWindow->m_queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            if (graphicsQueueNodeIndex == UINT32_MAX) {
                graphicsQueueNodeIndex = i;
            }

            if (supportsPresent[i] == VK_TRUE) {
                graphicsQueueNodeIndex = i;
                presentQueueNodeIndex = i;
                break;
            }
        }
    }
    if (presentQueueNodeIndex == UINT32_MAX) {
        // If didn't find a queue that supports both graphics and present, then
        // find a separate present queue.
        for (i = 0; i < pWindow->m_queue_count; ++i) {
            if (supportsPresent[i] == VK_TRUE) {
                presentQueueNodeIndex = i;
                break;
            }
        }
    }
    free(supportsPresent);

    // Generate resultor if could not find both a graphics and a present queue
    if (graphicsQueueNodeIndex == UINT32_MAX ||
        presentQueueNodeIndex == UINT32_MAX) {
        Fl::fatal("Could not find a graphics and a present queue\n",
                  "Swapchain Initialization Failure");
    }

    // TODO: Add support for separate queues, including presentation,
    //       synchronization, and appropriate tracking for QueueSubmit.
    // NOTE: While it is possible for an application to use a separate graphics
    //       and a present queues, this demo program assumes it is only using
    //       one:
    if (graphicsQueueNodeIndex != presentQueueNodeIndex) {
        Fl::fatal("Could not find a common graphics and a present queue\n",
                  "Swapchain Initialization Failure");
    }

    pWindow->m_graphics_queue_node_index = graphicsQueueNodeIndex;

    demo_init_device(pWindow);

    vkGetDeviceQueue(pWindow->m_device, pWindow->m_graphics_queue_node_index, 0,
                     &pWindow->m_queue);

    // Get the list of VkFormat's that are supported:
    uint32_t formatCount;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(pWindow->m_gpu, pWindow->m_surface,
                                               &formatCount, NULL);
    VK_CHECK_RESULT(result);
    VkSurfaceFormatKHR *surfFormats =
        (VkSurfaceFormatKHR *)VK_ALLOC(formatCount * sizeof(VkSurfaceFormatKHR));
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(pWindow->m_gpu, pWindow->m_surface,
                                               &formatCount, surfFormats);
    VK_CHECK_RESULT(result);
    // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
    // the surface has no prefresulted format.  Otherwise, at least one
    // supported format will be returned.
    if (formatCount == 1 && surfFormats[0].format == VK_FORMAT_UNDEFINED) {
        pWindow->m_format = VK_FORMAT_B8G8R8A8_UNORM;
    } else {
        assert(formatCount >= 1);
        pWindow->m_format = surfFormats[0].format;
    }
    pWindow->m_color_space = surfFormats[0].colorSpace;

    // Get Memory information and properties
    vkGetPhysicalDeviceMemoryProperties(pWindow->m_gpu,
                                        &pWindow->m_memory_properties);
}



static void demo_prepare(Fl_Vk_Window* pWindow)
{
    VkResult result;

    VkCommandPoolCreateInfo cmd_pool_info = {};
    cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_info.pNext = NULL;
    cmd_pool_info.queueFamilyIndex = pWindow->m_graphics_queue_node_index;
    cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    
    result = vkCreateCommandPool(pWindow->m_device, &cmd_pool_info, NULL,
                                 &pWindow->m_cmd_pool);
    VK_CHECK_RESULT(result);
    

    VkCommandBufferAllocateInfo cmd = {};
    cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd.pNext = NULL;
    cmd.commandPool = pWindow->m_cmd_pool;
    cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd.commandBufferCount = 1;
    
    result = vkAllocateCommandBuffers(pWindow->m_device, &cmd, &pWindow->m_draw_cmd);
    VK_CHECK_RESULT(result);
    
    demo_prepare_buffers(pWindow);
    if (pWindow->m_swapchain == VK_NULL_HANDLE) {
        printf("Swapchain recreation failed in resize\n");
        return; // Skip further setup if swapchain fails
    }
    demo_prepare_depth(pWindow);
    demo_prepare_textures(pWindow);
    demo_prepare_vertices(pWindow);
    demo_prepare_descriptor_layout(pWindow);
    demo_prepare_render_pass(pWindow);
    demo_prepare_pipeline(pWindow);
    
    demo_prepare_descriptor_pool(pWindow);
    demo_prepare_descriptor_set(pWindow);
    demo_prepare_framebuffers(pWindow);
}



void 
Fl_Vk_Window_Driver::init_vk()
{
    demo_init_vk(pWindow);
}

void 
Fl_Vk_Window_Driver::init_vk_swapchain()
{
    demo_init_vk_swapchain(pWindow);
}

void
Fl_Vk_Window_Driver::prepare()
{
    demo_prepare(pWindow);
}

void
Fl_Vk_Window_Driver::resize()
{
    if (!pWindow || !pWindow->m_device)
        return;

    uint32_t i;
    VkResult result;
    VkDevice device = pWindow->m_device;
    VkSwapchainKHR oldSwapchain = pWindow->m_swapchain;
    
    // Wait for all GPU operations to complete before resizing
    result = vkDeviceWaitIdle(device);
    VK_CHECK_RESULT(result);
    

    // In order to properly resize the window, we must re-create the swapchain
    // AND redo the command buffers, etc.
    
    // First, perform part of the demo_cleanup() function:
    for (i = 0; i < pWindow->m_swapchainImageCount; i++) {
        vkDestroyFramebuffer(pWindow->m_device, pWindow->m_framebuffers[i], NULL);
    }
    free(pWindow->m_framebuffers);
    pWindow->m_framebuffers = nullptr;
    
    vkDestroyDescriptorPool(pWindow->m_device, pWindow->m_desc_pool, NULL);
    pWindow->m_desc_pool = VK_NULL_HANDLE;

    if (pWindow->m_setup_cmd) {
        vkFreeCommandBuffers(pWindow->m_device, pWindow->m_cmd_pool, 1, &pWindow->m_setup_cmd);
        pWindow->m_setup_cmd = VK_NULL_HANDLE;
    }
    vkFreeCommandBuffers(pWindow->m_device, pWindow->m_cmd_pool, 1, &pWindow->m_draw_cmd);
    pWindow->m_draw_cmd = VK_NULL_HANDLE;
    
    vkDestroyCommandPool(pWindow->m_device, pWindow->m_cmd_pool, NULL);
    pWindow->m_cmd_pool = VK_NULL_HANDLE;

    vkDestroyPipeline(pWindow->m_device, pWindow->m_pipeline, NULL);
    pWindow->m_pipeline = VK_NULL_HANDLE;
    
    vkDestroyRenderPass(pWindow->m_device, pWindow->m_renderPass, NULL);
    pWindow->m_renderPass = VK_NULL_HANDLE;

    vkDestroyPipelineLayout(pWindow->m_device, pWindow->m_pipeline_layout, NULL);
    pWindow->m_pipeline_layout = VK_NULL_HANDLE;
    
    vkDestroyDescriptorSetLayout(pWindow->m_device, pWindow->m_desc_layout, NULL);
    pWindow->m_desc_layout = VK_NULL_HANDLE;
    

    vkDestroyBuffer(pWindow->m_device, pWindow->m_vertices.buf, NULL);
    vkFreeMemory(pWindow->m_device, pWindow->m_vertices.mem, NULL);
    
    for (i = 0; i < DEMO_TEXTURE_COUNT; i++) {
        vkDestroyImageView(pWindow->m_device, pWindow->m_textures[i].view, NULL);
        vkDestroyImage(pWindow->m_device, pWindow->m_textures[i].image, NULL);
        vkFreeMemory(pWindow->m_device, pWindow->m_textures[i].mem, NULL);
        vkDestroySampler(pWindow->m_device, pWindow->m_textures[i].sampler, NULL);
    }

    for (i = 0; i < pWindow->m_swapchainImageCount; i++) {
        vkDestroyImageView(pWindow->m_device, pWindow->m_buffers[i].view, NULL);
    }
    free(pWindow->m_buffers);
    pWindow->m_buffers = nullptr;

    vkDestroyImageView(pWindow->m_device, pWindow->m_depth.view, NULL);
    vkDestroyImage(pWindow->m_device, pWindow->m_depth.image, NULL);
    vkFreeMemory(pWindow->m_device, pWindow->m_depth.mem, NULL);

    // Recreate resources with new size
    prepare();
}

SwapChainSupportDetails
Fl_Vk_Window_Driver::query_swap_chain_support(VkPhysicalDevice physicalDevice)
{
    SwapChainSupportDetails details;

    // // 1. Get Surface Capabilities
    // vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice,
    //                                           pWindow->m_surface,
    //                                           &details.capabilities);

    // // 2. Get Surface Formats
    // uint32_t formatCount;
    // vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice,
    //                                      pWindow->m_surface,
    //                                      &formatCount, nullptr);

    // if (formatCount != 0) {
    //     details.formats.resize(formatCount);
    //     vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice,
    //                                          pWindow->m_surface,
    //                                          &formatCount,
    //                                          details.formats.data());
    // }

    // // 3. Get Present Modes
    // uint32_t presentModeCount;
    // vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice,
    //                                           pWindow->m_surface,
    //                                           &presentModeCount, nullptr);

    // if (presentModeCount != 0) {
    //     details.presentModes.resize(presentModeCount);
    //     vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice,
    //                                               pWindow->m_surface, &presentModeCount, details.presentModes.data());
    // }

    return details;
}

void Fl_Vk_Window_Driver::init_vulkan()
{
}

void Fl_Vk_Window_Driver::cleanup_vulkan()
{
}


void Fl_Vk_Window_Driver::swap_buffers()
{
}
