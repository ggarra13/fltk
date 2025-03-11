
#include "Fl_Vk_Window_Driver.H"

#include <cassert>
#include <set>
#include <stdexcept>
#include <vector>


#define FLTK_ALLOC(x)  (void*)malloc(x)
#define FLTK_ASSERT(x) assert(x)
#define FLTK_UNUSED(x) (void)(x)
#define FLTK_FREE(x) free(x);
#define FLTK_ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define FLTK_DBG() std::cerr << __FUNCTION__ << " " << __LINE__ << std::endl;

struct Fl_Vk_Frame;

bool g_FunctionsLoaded = true;

// Helpers

/*
 * Return 1 (true) if all layer names specified in check_names
 * can be found in given layer properties.
 */
static VkBool32 demo_check_layers(uint32_t check_count, const char **check_names,
                                  uint32_t layer_count,
                                  VkLayerProperties *layers) {
    uint32_t i, j;
    for (i = 0; i < check_count; i++) {
        VkBool32 found = 0;
        for (j = 0; j < layer_count; j++) {
            std::cerr << layers[j].layerName << std::endl;
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

void ImGui_ImplVulkanH_DestroyFrame(VkDevice device, Fl_Vk_Frame* fd, const VkAllocationCallbacks* allocator)
{
    vkDestroyFence(device, fd->Fence, allocator);
    vkFreeCommandBuffers(device, fd->CommandPool, 1, &fd->CommandBuffer);
    vkDestroyCommandPool(device, fd->CommandPool, allocator);
    fd->Fence = VK_NULL_HANDLE;
    fd->CommandBuffer = VK_NULL_HANDLE;
    fd->CommandPool = VK_NULL_HANDLE;

    vkDestroyImageView(device, fd->BackbufferView, allocator);
    vkDestroyFramebuffer(device, fd->Framebuffer, allocator);
}

void ImGui_ImplVulkanH_DestroyFrameSemaphores(VkDevice device, Fl_Vk_FrameSemaphores* fsd, const VkAllocationCallbacks* allocator)
{
    vkDestroySemaphore(device, fsd->ImageAcquiredSemaphore, allocator);
    vkDestroySemaphore(device, fsd->RenderCompleteSemaphore, allocator);
    fsd->ImageAcquiredSemaphore = fsd->RenderCompleteSemaphore = VK_NULL_HANDLE;
}

// void                 ImGui_ImplVulkanH_CreateOrResizeWindow(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device, Fl_Vk_Window* wnd, uint32_t queue_family, const VkAllocationCallbacks* allocator, int w, int h, uint32_t min_image_count);
// void                 ImGui_ImplVulkanH_DestroyWindow(VkInstance instance, VkDevice device, Fl_Vk_Window* wnd, const VkAllocationCallbacks* allocator);
// VkSurfaceFormatKHR   ImGui_ImplVulkanH_SelectSurfaceFormat(VkPhysicalDevice physical_device, VkSurfaceKHR surface, const VkFormat* request_formats, int request_formats_count, VkColorSpaceKHR request_color_space);
// VkPresentModeKHR     ImGui_ImplVulkanH_SelectPresentMode(VkPhysicalDevice physical_device, VkSurfaceKHR surface, const VkPresentModeKHR* request_modes, int request_modes_count);
// int                  ImGui_ImplVulkanH_GetMinImageCountFromPresentMode(VkPresentModeKHR present_mode);

// VkSurfaceFormatKHR ImGui_ImplVulkanH_SelectSurfaceFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, const VkFormat* request_formats, int request_formats_count, VkColorSpaceKHR request_color_space)
// {
//     // \@todo:
//     //
//     FLTK_ASSERT(g_FunctionsLoaded && "Need to call ImGui_ImplVulkan_LoadFunctions() if IMGUI_IMPL_VULKAN_NO_PROTOTYPES or VK_NO_PROTOTYPES are set!");
//     FLTK_ASSERT(request_formats != nullptr);
//     FLTK_ASSERT(request_formats_count > 0);

//     // Per Spec Format and View Format are expected to be the same unless VK_IMAGE_CREATE_MUTABLE_BIT was set at image creation
//     // Assuming that the default behavior is without setting this bit, there is no need for separate pWindow->m_swapchain image and image view format
//     // Additionally several new color spaces were introduced with Vulkan Spec v1.0.40,
//     // hence we must make sure that a format with the mostly available color space, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, is found and used.
//     uint32_t avail_count;
//     vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &avail_count, nullptr);
//     std::vector<VkSurfaceFormatKHR> avail_format;
//     avail_format.resize((int)avail_count);
//     vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &avail_count, avail_format.data());

//     // First check if only one format, VK_FORMAT_UNDEFINED, is available, which would imply that any format is available
//     if (avail_count == 1)
//     {
//         if (avail_format[0].format == VK_FORMAT_UNDEFINED)
//         {
//             VkSurfaceFormatKHR ret;
//             ret.format = request_formats[0];
//             ret.colorSpace = request_color_space;
//             return ret;
//         }
//         else
//         {
//             // No point in searching another format
//             return avail_format[0];
//         }
//     }
//     else
//     {
//         // Request several formats, the first found will be used
//         for (int request_i = 0; request_i < request_formats_count; request_i++)
//             for (uint32_t avail_i = 0; avail_i < avail_count; avail_i++)
//                 if (avail_format[avail_i].format == request_formats[request_i] && avail_format[avail_i].colorSpace == request_color_space)
//                     return avail_format[avail_i];

//         // If none of the requested image formats could be found, use the first available
//         return avail_format[0];
//     }
// }

// VkPresentModeKHR ImGui_ImplVulkanH_SelectPresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, const VkPresentModeKHR* request_modes, int request_modes_count)
// {
//     FLTK_ASSERT(g_FunctionsLoaded && "Need to call ImGui_ImplVulkan_LoadFunctions() if IMGUI_IMPL_VULKAN_NO_PROTOTYPES or VK_NO_PROTOTYPES are set!");
//     FLTK_ASSERT(request_modes != nullptr);
//     FLTK_ASSERT(request_modes_count > 0);

//     // Request a certain mode and confirm that it is available. If not use VK_PRESENT_MODE_FIFO_KHR which is mandatory
//     uint32_t avail_count = 0;
//     vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &avail_count, nullptr);
//     std::vector<VkPresentModeKHR> avail_modes;
//     avail_modes.resize((int)avail_count);
//     vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &avail_count,
//                                               avail_modes.data());
//     //for (uint32_t avail_i = 0; avail_i < avail_count; avail_i++)
//     //    printf("[vulkan] avail_modes[%d] = %d\n", avail_i, avail_modes[avail_i]);

//     for (int request_i = 0; request_i < request_modes_count; request_i++)
//         for (uint32_t avail_i = 0; avail_i < avail_count; avail_i++)
//             if (request_modes[request_i] == avail_modes[avail_i])
//                 return request_modes[request_i];

//     return VK_PRESENT_MODE_FIFO_KHR; // Always available
// }

void ImGui_ImplVulkanH_CreateWindowCommandBuffers(VkPhysicalDevice physicalDevice, VkDevice device, Fl_Vk_Window* pWindow, uint32_t queue_family, const VkAllocationCallbacks* allocator)
{
    FLTK_ASSERT(physicalDevice != VK_NULL_HANDLE && device != VK_NULL_HANDLE);
    FLTK_UNUSED(physicalDevice);

    // Create Command Buffers
    VkResult err;
    for (uint32_t i = 0; i < pWindow->m_swapchainImageCount; i++)
    {
        Fl_Vk_Frame* fd = &pWindow->m_frames[i];
        {
            VkCommandPoolCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            info.flags = 0;
            info.queueFamilyIndex = queue_family;
            err = vkCreateCommandPool(device, &info, allocator, &fd->CommandPool);
            VK_CHECK_RESULT(err);
        }
        {
            VkCommandBufferAllocateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            info.commandPool = fd->CommandPool;
            info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            info.commandBufferCount = 1;
            err = vkAllocateCommandBuffers(device, &info, &fd->CommandBuffer);
            VK_CHECK_RESULT(err);
        }
        {
            VkFenceCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            err = vkCreateFence(device, &info, allocator, &fd->Fence);
            VK_CHECK_RESULT(err);
        }
    }

    for (uint32_t i = 0; i < pWindow->m_semaphoreCount; i++)
    {
        Fl_Vk_FrameSemaphores* fsd = &pWindow->m_frameSemaphores[i];
        {
            VkSemaphoreCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            err = vkCreateSemaphore(device, &info, allocator, &fsd->ImageAcquiredSemaphore);
            VK_CHECK_RESULT(err);
            err = vkCreateSemaphore(device, &info, allocator, &fsd->RenderCompleteSemaphore);
            VK_CHECK_RESULT(err);
        }
    }
}

int ImGui_ImplVulkanH_GetMinImageCountFromPresentMode(VkPresentModeKHR present_mode)
{
    if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
        return 3;
    if (present_mode == VK_PRESENT_MODE_FIFO_KHR || present_mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR)
        return 2;
    if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
        return 1;
    FLTK_ASSERT(0);
    return 1;
}


static void demo_prepare_buffers(Fl_Vk_Window* pWindow) {
    VkResult err;
    VkSwapchainKHR oldSwapchain = pWindow->m_swapchain;

    // Check the surface capabilities and formats
    VkSurfaceCapabilitiesKHR surfCapabilities;
    err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        pWindow->m_physicalDevice, pWindow->m_surface, &surfCapabilities);
    VK_CHECK_RESULT(err);
    
    uint32_t presentModeCount;
    err = vkGetPhysicalDeviceSurfacePresentModesKHR(
        pWindow->m_physicalDevice, pWindow->m_surface, &presentModeCount, NULL);
    VK_CHECK_RESULT(err);
    
    VkPresentModeKHR *presentModes =
        (VkPresentModeKHR *)malloc(presentModeCount * sizeof(VkPresentModeKHR));
    assert(presentModes);
    err = vkGetPhysicalDeviceSurfacePresentModesKHR(
        pWindow->m_physicalDevice, pWindow->m_surface, &presentModeCount, presentModes);
    VK_CHECK_RESULT(err);

    VkExtent2D swapchainExtent;
    // width and height are either both 0xFFFFFFFF, or both not 0xFFFFFFFF.
    if (surfCapabilities.currentExtent.width == 0xFFFFFFFF) {
        // If the surface size is undefined, the size is set to the size
        // of the images requested, which must fit within the minimum and
        // maximum values.
        swapchainExtent.width = pWindow->w();
        swapchainExtent.height = pWindow->h();

        if (swapchainExtent.width < surfCapabilities.minImageExtent.width) {
            swapchainExtent.width = surfCapabilities.minImageExtent.width;
        } else if (swapchainExtent.width > surfCapabilities.maxImageExtent.width) {
            swapchainExtent.width = surfCapabilities.maxImageExtent.width;
        }

        if (swapchainExtent.height < surfCapabilities.minImageExtent.height) {
            swapchainExtent.height = surfCapabilities.minImageExtent.height;
        } else if (swapchainExtent.height > surfCapabilities.maxImageExtent.height) {
            swapchainExtent.height = surfCapabilities.maxImageExtent.height;
        }
    } else {
        // If the surface size is defined, the swap chain size must match
        swapchainExtent = surfCapabilities.currentExtent;
        // pWindow->w = surfCapabilities.currentExtent.width;
        // pWindow->h = surfCapabilities.currentExtent.height;
        pWindow->Fl_Widget::size(surfCapabilities.currentExtent.width,
                                 surfCapabilities.currentExtent.height);
    }

    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

    // Determine the number of VkImage's to use in the swap chain.
    // Application desires to only acquire 1 image at a time (which is
    // "surfCapabilities.minImageCount").
    uint32_t desiredNumOfSwapchainImages = surfCapabilities.minImageCount;
    // If maxImageCount is 0, we can ask for as many images as we want;
    // otherwise we're limited to maxImageCount
    if ((surfCapabilities.maxImageCount > 0) &&
        (desiredNumOfSwapchainImages > surfCapabilities.maxImageCount)) {
        // Application must settle for fewer images than desired:
        desiredNumOfSwapchainImages = surfCapabilities.maxImageCount;
    }

    VkSurfaceTransformFlagsKHR preTransform;
    if (surfCapabilities.supportedTransforms &
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        preTransform = surfCapabilities.currentTransform;
    }

    VkSwapchainCreateInfoKHR swapchain = {};
    swapchain.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain.pNext = NULL;
    swapchain.surface = pWindow->m_surface;
    swapchain.minImageCount = desiredNumOfSwapchainImages;
    swapchain.imageFormat = pWindow->m_format;
    swapchain.imageColorSpace = pWindow->m_color_space;
    swapchain.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain.preTransform =
        static_cast<VkSurfaceTransformFlagBitsKHR>(preTransform);
    swapchain.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain.imageArrayLayers = 1;
    swapchain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain.queueFamilyIndexCount = 0;
    swapchain.pQueueFamilyIndices = NULL;
    swapchain.presentMode = swapchainPresentMode;
    swapchain.oldSwapchain = oldSwapchain;
    swapchain.clipped = true;
    swapchain.imageExtent.width = swapchainExtent.width;
    swapchain.imageExtent.height = swapchainExtent.height;
    
    uint32_t i;

    err = vkCreateSwapchainKHR(pWindow->m_device, &swapchain, NULL, &pWindow->m_swapchain);
    VK_CHECK_RESULT(err);

    // If we just re-created an existing swapchain, we should destroy the old
    // swapchain at this point.
    // Note: destroying the swapchain also cleans up all its associated
    // presentable images once the platform is done with them.
    if (oldSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(pWindow->m_device, oldSwapchain, NULL);
    }

    err = vkGetSwapchainImagesKHR(pWindow->m_device, pWindow->m_swapchain,
                                        &pWindow->m_swapchainImageCount, NULL);
    VK_CHECK_RESULT(err);

    VkImage *swapchainImages =
        (VkImage *)malloc(pWindow->m_swapchainImageCount * sizeof(VkImage));
    assert(swapchainImages);
    err = vkGetSwapchainImagesKHR(pWindow->m_device, pWindow->m_swapchain,
                                  &pWindow->m_swapchainImageCount,
                                  swapchainImages);
    VK_CHECK_RESULT(err);

    pWindow->m_buffers = (SwapchainBuffers*)
                         FLTK_ALLOC(sizeof(SwapchainBuffers) *
                                    pWindow->m_swapchainImageCount);
    assert(pWindow->m_buffers);

    for (i = 0; i < pWindow->m_swapchainImageCount; i++) {
        VkImageViewCreateInfo color_attachment_view = {};
        color_attachment_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        color_attachment_view.pNext = NULL;
        color_attachment_view.format = pWindow->m_format;
        color_attachment_view.components.r = VK_COMPONENT_SWIZZLE_R;
        color_attachment_view.components.g = VK_COMPONENT_SWIZZLE_G;
        color_attachment_view.components.b = VK_COMPONENT_SWIZZLE_B;
        color_attachment_view.components.a = VK_COMPONENT_SWIZZLE_A;
        color_attachment_view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        color_attachment_view.subresourceRange.baseMipLevel = 0;
        color_attachment_view.subresourceRange.levelCount = 1;
        color_attachment_view.subresourceRange.baseArrayLayer = 0;
        color_attachment_view.subresourceRange.layerCount = 1;
        color_attachment_view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        color_attachment_view.flags = 0;

        pWindow->m_buffers[i].image = swapchainImages[i];

        color_attachment_view.image = pWindow->m_buffers[i].image;

        err = vkCreateImageView(pWindow->m_device, &color_attachment_view, NULL,
                                &pWindow->m_buffers[i].view);
        VK_CHECK_RESULT(err);
    }

    pWindow->m_current_buffer = 0;

    if (NULL != presentModes) {
        free(presentModes);
    }
}

// Also destroy old swap chain and in-flight frames data, if any.
void ImGui_ImplVulkanH_CreateWindowSwapChain(VkPhysicalDevice physicalDevice, VkDevice device, Fl_Vk_Window* pWindow, const VkAllocationCallbacks* allocator, int w, int h, uint32_t min_image_count)
{
    std::cerr << "WxH=" << w << "x" << h << " count=" << min_image_count
              << std::endl;
    VkResult err;
    VkSwapchainKHR old_swapchain = pWindow->m_swapchain;
    pWindow->m_swapchain = VK_NULL_HANDLE;

    FLTK_DBG();
    
    err = vkDeviceWaitIdle(device);
    VK_CHECK_RESULT(err);

    FLTK_DBG();
    // We don't use ImGui_ImplVulkanH_DestroyWindow() because we want to preserve the old swapchain to create the new one.
    // Destroy old Framebuffer
    for (uint32_t i = 0; i < pWindow->m_swapchainImageCount; i++)
        ImGui_ImplVulkanH_DestroyFrame(device, &pWindow->m_frames[i], allocator);
    for (uint32_t i = 0; i < pWindow->m_semaphoreCount; i++)
        ImGui_ImplVulkanH_DestroyFrameSemaphores(device, &pWindow->m_frameSemaphores[i], allocator);
    
    FLTK_DBG();
    
    FLTK_FREE(pWindow->m_frames);
    FLTK_FREE(pWindow->m_frameSemaphores);
    
    FLTK_DBG();
    
    pWindow->m_frames = nullptr;
    pWindow->m_frameSemaphores = nullptr;
    pWindow->m_swapchainImageCount = 0;
    
    FLTK_DBG();
    if (pWindow->m_renderPass != VK_NULL_HANDLE)
        vkDestroyRenderPass(device, pWindow->m_renderPass, pWindow->m_allocator);
    FLTK_DBG();
    if (pWindow->m_pipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(device, pWindow->m_pipeline, pWindow->m_allocator);

    // If min image count was not specified, request different count of images dependent on selected present mode
    if (min_image_count == 0)
        min_image_count = ImGui_ImplVulkanH_GetMinImageCountFromPresentMode(pWindow->m_presentMode);

    // Create Swapchain
    {
    FLTK_DBG();
        VkSwapchainCreateInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = pWindow->m_surface;
        info.minImageCount = min_image_count;
        info.imageFormat = pWindow->m_surfaceFormat.format;
        info.imageColorSpace = pWindow->m_surfaceFormat.colorSpace;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;           // Assume that graphics family == present family
        info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = pWindow->m_presentMode;
        info.clipped = VK_TRUE;
        info.oldSwapchain = old_swapchain;
        VkSurfaceCapabilitiesKHR cap = {};
        err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice,
                                                        pWindow->m_surface,
                                                        &cap);
        VK_CHECK_RESULT(err);
        if (info.minImageCount < cap.minImageCount)
            info.minImageCount = cap.minImageCount;
        else if (cap.maxImageCount != 0 && info.minImageCount > cap.maxImageCount)
            info.minImageCount = cap.maxImageCount;
        std::cerr << "info.minImageCount=" << info.minImageCount
                  << std::endl;
        std::cerr << "cap.maxImageCount=" << cap.maxImageCount
                  << std::endl;

        if (cap.currentExtent.width == 0xffffffff)
        {
            info.imageExtent.width = w;
            info.imageExtent.height = h;
        }
        else
        {
            info.imageExtent.width = cap.currentExtent.width;
            info.imageExtent.height = cap.currentExtent.height;
        }
        pWindow->Fl_Widget::size(info.imageExtent.width,
                                 info.imageExtent.height);
            
        FLTK_DBG();
        err = vkCreateSwapchainKHR(device, &info, allocator, &pWindow->m_swapchain);
        VK_CHECK_RESULT(err);
        err = vkGetSwapchainImagesKHR(device, pWindow->m_swapchain,
                                      &pWindow->m_swapchainImageCount, nullptr);
        VK_CHECK_RESULT(err);
        VkImage backbuffers[16] = {};
        FLTK_ASSERT(pWindow->ImageCount >= min_image_count);
        FLTK_ASSERT(pWindow->ImageCount < FLTK_ARRAYSIZE(backbuffers));
        err = vkGetSwapchainImagesKHR(device, pWindow->m_swapchain,
                                      &pWindow->m_swapchainImageCount, backbuffers);
        VK_CHECK_RESULT(err);

        FLTK_ASSERT(pWindow->Frames == nullptr &&
                    pWindow->FrameSemaphores == nullptr);
        FLTK_DBG();
        pWindow->m_semaphoreCount = pWindow->m_swapchainImageCount + 1;
        pWindow->m_frames = (Fl_Vk_Frame*)FLTK_ALLOC(sizeof(Fl_Vk_Frame) * pWindow->m_swapchainImageCount);
        pWindow->m_frameSemaphores = (Fl_Vk_FrameSemaphores*)FLTK_ALLOC(sizeof(Fl_Vk_FrameSemaphores) * pWindow->m_semaphoreCount);
        memset(pWindow->m_frames, 0, sizeof(pWindow->m_frames[0]) * pWindow->m_swapchainImageCount);
        FLTK_DBG();
        memset(pWindow->m_frameSemaphores, 0, sizeof(pWindow->m_frameSemaphores[0]) * pWindow->m_semaphoreCount);
        for (uint32_t i = 0; i < pWindow->m_swapchainImageCount; i++)
            pWindow->m_frames[i].Backbuffer = backbuffers[i];
    }
    FLTK_DBG();
    if (old_swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(device, old_swapchain, allocator);

    // Create the Render Pass
    if (pWindow->m_useDynamicRendering == false)
    {
        FLTK_DBG();
        VkAttachmentDescription attachment = {};
        attachment.format = pWindow->m_surfaceFormat.format;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = pWindow->m_clearEnable ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference color_attachment = {};
        color_attachment.attachment = 0;
        color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;
        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments = &attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;
        err = vkCreateRenderPass(device, &info, allocator, &pWindow->m_renderPass);
        VK_CHECK_RESULT(err);

        // We do not create a pipeline by default as this is also used by examples' main.cpp,
        // but secondary viewport in multi-viewport mode may want to create one with:
        //ImGui_ImplVulkan_CreatePipeline(device, allocator, VK_NULL_HANDLE, pWindow->RenderPass, VK_SAMPLE_COUNT_1_BIT, &pWindow->Pipeline, v->Subpass);
    }

    // Create The Image Views
    {
    FLTK_DBG();
        VkImageViewCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = pWindow->m_surfaceFormat.format;
        info.components.r = VK_COMPONENT_SWIZZLE_R;
        info.components.g = VK_COMPONENT_SWIZZLE_G;
        info.components.b = VK_COMPONENT_SWIZZLE_B;
        info.components.a = VK_COMPONENT_SWIZZLE_A;
        VkImageSubresourceRange image_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        info.subresourceRange = image_range;
        for (uint32_t i = 0; i < pWindow->m_swapchainImageCount; i++)
        {
            Fl_Vk_Frame* fd = &pWindow->m_frames[i];
            info.image = fd->Backbuffer;
            err = vkCreateImageView(device, &info, allocator, &fd->BackbufferView);
            VK_CHECK_RESULT(err);
        }
    }

    // Create Framebuffer
    if (pWindow->m_useDynamicRendering == false)
    {
    FLTK_DBG();
        VkImageView attachment[1];
        VkFramebufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = pWindow->m_renderPass;
        info.attachmentCount = 1;
        info.pAttachments = attachment;
        info.width = pWindow->w();
        info.height = pWindow->h();
        info.layers = 1;
        for (uint32_t i = 0; i < pWindow->m_swapchainImageCount; i++)
        {
            Fl_Vk_Frame* fd = &pWindow->m_frames[i];
            attachment[0] = fd->BackbufferView;
            err = vkCreateFramebuffer(device, &info, allocator, &fd->Framebuffer);
            VK_CHECK_RESULT(err);
        }
    }
}

// // Create or resize window
// void ImGui_ImplVulkanH_CreateOrResizeWindow(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, Fl_Vk_Window* pWindow, uint32_t queue_family, const VkAllocationCallbacks* allocator, int width, int height, uint32_t min_image_count)
// {
//     //FLTK_ASSERT(g_FunctionsLoaded && "Need to call ImGui_ImplVulkan_LoadFunctions() if IMGUI_IMPL_VULKAN_NO_PROTOTYPES or VK_NO_PROTOTYPES are set!");
//     (void)instance;
//     std::cerr << __FUNCTION__ << " "  << __LINE__ << std::endl;
//     ImGui_ImplVulkanH_CreateWindowSwapChain(physicalDevice, device, pWindow, allocator, width, height, min_image_count);
//     std::cerr << __FUNCTION__ << " "  << __LINE__ << std::endl;
//     ImGui_ImplVulkanH_CreateWindowCommandBuffers(physicalDevice, device, pWindow, queue_family, allocator);
//     std::cerr << __FUNCTION__ << " "  << __LINE__ << std::endl;
// }

// void ImGui_ImplVulkanH_DestroyWindow(VkInstance instance, VkDevice device, Fl_Vk_Window*& pWindow, const VkAllocationCallbacks* allocator)
// {
//     vkDeviceWaitIdle(device); // FIXME: We could wait on the Queue if we had the queue in pWindow-> (otherwise VulkanH functions can't use globals)
//     //vkQueueWaitIdle(bd->Queue);

//     for (uint32_t i = 0; i < pWindow->ImageCount; i++)
//         ImGui_ImplVulkanH_DestroyFrame(device, &pWindow->Frames[i], allocator);
//     for (uint32_t i = 0; i < pWindow->SemaphoreCount; i++)
//         ImGui_ImplVulkanH_DestroyFrameSemaphores(device, &pWindow->FrameSemaphores[i], allocator);
//     FLTK_FREE(pWindow->Frames);
//     FLTK_FREE(pWindow->FrameSemaphores);
//     pWindow->Frames = nullptr;
//     pWindow->FrameSemaphores = nullptr;
//     vkDestroyPipeline(device, pWindow->pipeline(), allocator);
//     vkDestroyRenderPass(device, pWindow->renderPass(), allocator);
//     vkDestroySwapchainKHR(device, pWindow->swapchain(), allocator);
//     vkDestroySurfaceKHR(instance, pWindow->m_surface, allocator);

//     pWindow = nullptr;
// }

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
    VkResult err;

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

    err = vkCreateDevice(pWindow->m_physicalDevice, &device, NULL, &pWindow->m_device);
    assert(!err);
}

static void demo_init_vk_swapchain(Fl_Vk_Window* pWindow)
{
    VkResult err;
    uint32_t i;
    
    // Iterate over each queue to learn whether it supports presenting:
    VkBool32 *supportsPresent =
        (VkBool32 *)malloc(pWindow->m_queue_count * sizeof(VkBool32));
    for (i = 0; i < pWindow->m_queue_count; i++) {
        vkGetPhysicalDeviceSurfaceSupportKHR(pWindow->m_physicalDevice, i, pWindow->m_surface,
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

    // Generate error if could not find both a graphics and a present queue
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
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(pWindow->m_physicalDevice, pWindow->m_surface,
                                               &formatCount, NULL);
    assert(!err);
    VkSurfaceFormatKHR *surfFormats =
        (VkSurfaceFormatKHR *)malloc(formatCount * sizeof(VkSurfaceFormatKHR));
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(pWindow->m_physicalDevice, pWindow->m_surface,
                                               &formatCount, surfFormats);
    assert(!err);
    // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
    // the surface has no preferred format.  Otherwise, at least one
    // supported format will be returned.
    if (formatCount == 1 && surfFormats[0].format == VK_FORMAT_UNDEFINED) {
        pWindow->m_format = VK_FORMAT_B8G8R8A8_UNORM;
    } else {
        assert(formatCount >= 1);
        pWindow->m_format = surfFormats[0].format;
    }
    pWindow->m_color_space = surfFormats[0].colorSpace;

    pWindow->m_curFrame = 0;

    // Get Memory information and properties
    vkGetPhysicalDeviceMemoryProperties(pWindow->m_physicalDevice,
                                        &pWindow->m_memory_properties);
}


void
Fl_Vk_Window_Driver::resize(int width, int height,
                            uint32_t queue_family, uint32_t min_image_count)
{
    // In order to properly resize the window, we must re-create the swapchain
    // AND redo the command buffers, etc.
    //
    // First, perform part of the demo_cleanup() function:

    // Destroy old Framebuffer
    for (uint32_t i = 0; i < pWindow->m_swapchainImageCount; i++)
        ImGui_ImplVulkanH_DestroyFrame(pWindow->m_device, &pWindow->m_frames[i],
                                       pWindow->m_allocator);
    
    // Destroy old Semaphores
    for (uint32_t i = 0; i < pWindow->m_semaphoreCount; i++)
        ImGui_ImplVulkanH_DestroyFrameSemaphores(pWindow->m_device,
                                                 &pWindow->m_frameSemaphores[i],
                                                 pWindow->m_allocator);
    FLTK_FREE(pWindow->m_frames);
    FLTK_FREE(pWindow->m_frameSemaphores);
    pWindow->m_frames = nullptr;
    pWindow->m_frameSemaphores = nullptr;
    pWindow->m_swapchainImageCount = 0;
    
    demo_prepare_buffers(pWindow);

    // ImGui_ImplVulkanH_CreateWindowSwapChain(pWindow->m_physicalDevice,
    //                                         pWindow->m_device,
    //                                         pWindow,
    //                                         pWindow->m_allocator,
    //                                         width, height,
    //                                         min_image_count);
    // ImGui_ImplVulkanH_CreateWindowCommandBuffers(pWindow->m_physicalDevice,
    //                                              pWindow->m_device,
    //                                              pWindow, queue_family,
    //                                              pWindow->m_allocator);

     
    // \@todo: needed?
    // vkDestroyDescriptorPool(pWindow->m_device, pWindow->m_desc_pool, NULL);

    // if (pWindow->m_setup_cmd) {
    //     vkFreeCommandBuffers(pWindow->m_device, pWindow->m_cmd_pool, 1, &pWindow->m_setup_cmd);
    //     pWindow->m_setup_cmd = VK_NULL_HANDLE;
    // }
    // vkFreeCommandBuffers(pWindow->m_device, pWindow->m_cmd_pool, 1, &pWindow->m_draw_cmd);
    // vkDestroyCommandPool(pWindow->m_device, pWindow->m_cmd_pool, NULL);

    // vkDestroyPipeline(pWindow->m_device, pWindow->m_pipeline, NULL);
    // vkDestroyRenderPass(pWindow->m_device, pWindow->m_render_pass, NULL);
    // vkDestroyPipelineLayout(pWindow->m_device, pWindow->m_pipeline_layout, NULL);
    // vkDestroyDescriptorSetLayout(pWindow->m_device, pWindow->m_desc_layout, NULL);

    // vkDestroyBuffer(pWindow->m_device, pWindow->m_vertices.buf, NULL);
    // vkFreeMemory(pWindow->m_device, pWindow->m_vertices.mem, NULL);

    // for (i = 0; i < DEMO_TEXTURE_COUNT; i++) {
    //     vkDestroyImageView(pWindow->m_device, pWindow->m_textures[i].view, NULL);
    //     vkDestroyImage(pWindow->m_device, pWindow->m_textures[i].image, NULL);
    //     vkFreeMemory(pWindow->m_device, pWindow->m_textures[i].mem, NULL);
    //     vkDestroySampler(pWindow->m_device, pWindow->m_textures[i].sampler, NULL);
    // }

    // for (i = 0; i < pWindow->m_swapchainImageCount; i++) {
    //     vkDestroyImageView(pWindow->m_device, pWindow->m_buffers[i].view, NULL);
    // }

    // vkDestroyImageView(pWindow->m_device, pWindow->m_depth.view, NULL);
    // vkDestroyImage(pWindow->m_device, pWindow->m_depth.image, NULL);
    // vkFreeMemory(pWindow->m_device, pWindow->m_depth.mem, NULL);

    // free(pWindow->m_buffers);
    
    // ImGui_ImplVulkanH_CreateOrResizeWindow(pWindow->m_instance,
    //                                        pWindow->m_physicalDevice,
    //                                        pWindow->m_device,
    //                                        pWindow,
    //                                        queue_family,
    //                                        pWindow->m_allocator,
    //                                        width, height, min_image_count);
}

SwapChainSupportDetails
Fl_Vk_Window_Driver::query_swap_chain_support(VkPhysicalDevice physicalDevice)
{
    SwapChainSupportDetails details;

    // 1. Get Surface Capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice,
                                              pWindow->m_surface,
                                              &details.capabilities);

    // 2. Get Surface Formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice,
                                         pWindow->m_surface,
                                         &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice,
                                             pWindow->m_surface,
                                             &formatCount,
                                             details.formats.data());
    }

    // 3. Get Present Modes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice,
                                              pWindow->m_surface,
                                              &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice,
                                                  pWindow->m_surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

bool Fl_Vk_Window_Driver::is_device_suitable(VkPhysicalDevice physicalDevice)
{
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

    QueueFamilyIndices indices = find_queue_families(physicalDevice);

    bool extensionsSupported = check_device_extension_support(physicalDevice);

    bool swapChainAdequate = false;
    if (extensionsSupported)
    {
        SwapChainSupportDetails swapChainSupport = query_swap_chain_support(physicalDevice);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

void Fl_Vk_Window_Driver::init_vulkan()
{
    create_instance();
    // create_swap_chain();
    // create_command_pool();
    // create_command_buffers();
    // create_semaphores();
    // get_queues();
}

void Fl_Vk_Window_Driver::cleanup_vulkan()
{
}

void Fl_Vk_Window_Driver::create_instance()
{
    VkResult err;
    VkBool32 portability_enumeration = VK_FALSE;
    uint32_t i = 0;
    uint32_t required_extension_count = 0;
    uint32_t instance_extension_count = 0;
    uint32_t instance_layer_count = 0;
    uint32_t validation_layer_count = 0;
    const char **instance_validation_layers = NULL;
    uint32_t enabled_extension_count = 0;
    uint32_t enabled_layer_count = 0;
    const char *extension_names[64];
    const char *enabled_layers[64];

    VkBool32 validate = false;  // true; fails 
    
    char *instance_validation_layers_alt1[] = {
        "VK_LAYER_LUNARG_standard_validation"
    };

    char *instance_validation_layers_alt2[] = {
        "VK_LAYER_GOOGLE_threading",       "VK_LAYER_LUNARG_parameter_validation",
        "VK_LAYER_LUNARG_object_tracker",  "VK_LAYER_LUNARG_image",
        "VK_LAYER_LUNARG_core_validation", "VK_LAYER_LUNARG_swapchain",
        "VK_LAYER_GOOGLE_unique_objects"
    };

    /* Look for validation layers */
    VkBool32 validation_found = 0;
    if (validate) {

        err = vkEnumerateInstanceLayerProperties(&instance_layer_count, NULL);
        assert(!err);

        instance_validation_layers = (const char**) instance_validation_layers_alt1;
        if (instance_layer_count > 0) {
            VkLayerProperties *instance_layers =
                (VkLayerProperties*)malloc(sizeof (VkLayerProperties) * instance_layer_count);
            err = vkEnumerateInstanceLayerProperties(&instance_layer_count,
                                                     instance_layers);
            assert(!err);


            validation_found = demo_check_layers(
                FLTK_ARRAY_SIZE(instance_validation_layers_alt1),
                instance_validation_layers, instance_layer_count,
                instance_layers);
            if (validation_found) {
                enabled_layer_count = FLTK_ARRAY_SIZE(instance_validation_layers_alt1);
                enabled_layers[0] = "VK_LAYER_LUNARG_standard_validation";
                validation_layer_count = 1;
            } else {
                // use alternative set of validation layers
                instance_validation_layers =
                    (const char**) instance_validation_layers_alt2;
                enabled_layer_count = FLTK_ARRAY_SIZE(instance_validation_layers_alt2);
                validation_found = demo_check_layers(
                    FLTK_ARRAY_SIZE(instance_validation_layers_alt2),
                    instance_validation_layers, instance_layer_count,
                    instance_layers);
                validation_layer_count =
                    FLTK_ARRAY_SIZE(instance_validation_layers_alt2);
                for (i = 0; i < validation_layer_count; i++) {
                    enabled_layers[i] = instance_validation_layers[i];
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

    // /* Look for instance extensions */
    const std::vector<std::string>& required_extensions =
    get_required_extensions();
    if (required_extensions.empty()) {
        Fl::fatal("glfwGetRequiredInstanceExtensions failed to find the "
                  "platform surface extensions.\n\nDo you have a compatible "
                  "Vulkan installable client driver (ICD) installed?\nPlease "
                  "look at the Getting Started guide for additional "
                  "information.\n",
                  "vkCreateInstance Failure");
    }

    required_extension_count = required_extensions.size();
    for (i = 0; i < required_extension_count; i++) {
        extension_names[enabled_extension_count++] =
            required_extensions[i].c_str();
        assert(enabled_extension_count < 64);
    }

    err = vkEnumerateInstanceExtensionProperties(
        NULL, &instance_extension_count, NULL);
    assert(!err);

    if (instance_extension_count > 0) {
        VkExtensionProperties *instance_extensions =
            (VkExtensionProperties*)
            malloc(sizeof(VkExtensionProperties) * instance_extension_count);
        err = vkEnumerateInstanceExtensionProperties(
            NULL, &instance_extension_count, instance_extensions);
        assert(!err);
        for (i = 0; i < instance_extension_count; i++) {
            if (!strcmp(VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
                        instance_extensions[i].extensionName)) {
                if (validate) {
                    extension_names[enabled_extension_count++] =
                        VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
                }
            }
            assert(enabled_extension_count < 64);
            if (!strcmp(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
                        instance_extensions[i].extensionName)) {
                extension_names[enabled_extension_count++] =
                    VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
                portability_enumeration = VK_TRUE;
            }
            assert(enabled_extension_count < 64);
        }

        free(instance_extensions);
    }

    
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "FLTK";
    appInfo.applicationVersion = 0,
    appInfo.pEngineName = "FLTK";
    appInfo.engineVersion = 0;
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = enabled_layer_count; 
    createInfo.enabledExtensionCount = enabled_extension_count; 
    createInfo.ppEnabledExtensionNames = extension_names; 

    if (vkCreateInstance(&createInfo, nullptr, &pWindow->m_instance) != VK_SUCCESS) {
        Fl::fatal("failed to create instance!");
    }
}

void Fl_Vk_Window_Driver::create_logical_device()
{
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = pWindow->m_queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pEnabledFeatures = &deviceFeatures;

    std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkResult result = vkCreateDevice(pWindow->m_physicalDevice, &createInfo,
                                     nullptr, &pWindow->m_device);
    if (result != VK_SUCCESS)
    {
        Fl::fatal("Failed to create logical device!");
    }

    vkGetDeviceQueue(pWindow->m_device,
                     pWindow->m_queueFamilyIndex, 0,
                     &pWindow->m_queue);

    // Get the list of VkFormat's that are supported:
    uint32_t formatCount;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(pWindow->m_physicalDevice,
                                               pWindow->m_surface,
                                               &formatCount, NULL);
    VK_CHECK_RESULT(result);
    
    VkSurfaceFormatKHR *surfFormats =
        (VkSurfaceFormatKHR *)malloc(formatCount * sizeof(VkSurfaceFormatKHR));
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(pWindow->m_physicalDevice,
                                                  pWindow->m_surface,
                                                  &formatCount, surfFormats);
    VK_CHECK_RESULT(result);
    
    // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
    // the surface has no preferred format.  Otherwise, at least one
    // supported format will be returned.
    if (formatCount == 1 && surfFormats[0].format == VK_FORMAT_UNDEFINED) {
        pWindow->m_format = VK_FORMAT_B8G8R8A8_UNORM;
    } else {
        assert(formatCount >= 1);
        pWindow->m_format = surfFormats[0].format;
    }
    pWindow->m_color_space = surfFormats[0].colorSpace;

    // Get Memory information and properties
    vkGetPhysicalDeviceMemoryProperties(pWindow->m_physicalDevice,
                                        &pWindow->m_memory_properties);
}

void Fl_Vk_Window_Driver::pick_physical_device()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(pWindow->m_instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        Fl::fatal("failed to find GPUs with Vulkan support!");
    }
    
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(pWindow->m_instance,
                               &deviceCount, devices.data());

    for (const auto &device : devices)
    {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device,
                                                 &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device,
                                                 &queueFamilyCount,
                                                 queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilies.size(); i++)
        {
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
            {
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i,
                                                     pWindow->m_surface,
                                                     &presentSupport);
                if (presentSupport)
                {
                    pWindow->m_physicalDevice = device;
                    pWindow->m_queueFamilyIndex = i;
                    return;
                }
            }
        }
        
    }
    Fl::fatal("Failed to find a suitable GPU!");
}

void Fl_Vk_Window_Driver::create_swap_chain()
{
    std::cerr << __FUNCTION__ << " " << __LINE__ << std::endl;
}

void Fl_Vk_Window_Driver::create_command_pool()
{
    std::cerr << __FUNCTION__ << " " << __LINE__ << std::endl;
}

void Fl_Vk_Window_Driver::create_command_buffers()
{
    std::cerr << __FUNCTION__ << " " << __LINE__ << std::endl;
}

void Fl_Vk_Window_Driver::create_semaphores()
{
    std::cerr << __FUNCTION__ << " " << __LINE__ << std::endl;
}

void Fl_Vk_Window_Driver::get_queues()
{
    std::cerr << __FUNCTION__ << " " << __LINE__ << std::endl;
}

void Fl_Vk_Window_Driver::swap_buffers()
{
    std::cerr << __FUNCTION__ << " " << __LINE__ << std::endl;
}
