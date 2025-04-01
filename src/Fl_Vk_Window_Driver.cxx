
#include "FL/vk.h"
#include "Fl_Vk_Window_Driver.H"

#ifndef _WIN32
#include <signal.h>
#endif

#include <cassert>
#include <set>
#include <stdexcept>
#include <vector>

#ifndef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#define VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME "VK_KHR_portability_subset"
#endif

#ifndef VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME 
#define VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME "VK_KHR_get_physical_device_properties2"
#endif

// macros
#define CLAMP(v, vmin, vmax) (v < vmin ? vmin : (v > vmax ? vmax : v))
#define FLTK_ADD_EXTENSION(x) \
    if (!strcmp(x, device_extensions[i].extensionName)) { \
        pWindow->m_extension_names[pWindow->m_enabled_extension_count++] = x; \
        assert(pWindow->m_enabled_extension_count < 64); \
    }

bool g_FunctionsLoaded = true;

static int validation_error = 0;

//! Returns true or false if extension name is supported.
static bool isExtensionSupported(const char* extensionName) {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    for (const auto& ext : extensions) {
        if (strcmp(ext.extensionName, extensionName) == 0) {
            return true;
        }
    }
    return false;
}
/*
 * Return 1 (true) if all layer names specified in check_names
 * can be found in given layer properties.
 */
static VkBool32 check_layers(uint32_t check_count, const char **check_names, uint32_t layer_count,
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
static bool memory_type_from_properties(Fl_Vk_Window *pWindow, uint32_t typeBits,
                                        VkFlags requirements_mask, uint32_t *typeIndex) {
  uint32_t i;
  // Search memtypes to find first index with those properties
  for (i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
    if ((typeBits & 1) == 1) {
      // Type is available, does it match user properties?
      if ((pWindow->m_memory_properties.memoryTypes[i].propertyFlags & requirements_mask) ==
          requirements_mask) {
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
void Fl_Vk_Window_Driver::set_image_layout(VkImage image,
                                           VkImageAspectFlags aspectMask,
                                           VkImageLayout old_image_layout,
                                           VkImageLayout new_image_layout,
                                           int srcAccessMaskInt) {
  VkResult err;

  VkAccessFlagBits srcAccessMask = static_cast<VkAccessFlagBits>(srcAccessMaskInt);

  VkImageMemoryBarrier image_memory_barrier = {};
  image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  image_memory_barrier.pNext = NULL;
  image_memory_barrier.srcAccessMask = srcAccessMask;
  image_memory_barrier.dstAccessMask = 0;
  image_memory_barrier.oldLayout = old_image_layout;
  image_memory_barrier.newLayout = new_image_layout;
  image_memory_barrier.image = image;
  image_memory_barrier.subresourceRange = {aspectMask, 0, 1, 0, 1};

  VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_HOST_BIT;         // Default for host writes
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

  vkCmdPipelineBarrier(pWindow->m_setup_cmd, src_stages,
                       dest_stages, 0, 0, NULL, 0, NULL, 1,
                       &image_memory_barrier);
}

// Uses m_swapchain, m_swapchainImageCount, m_gpu, m_surface, m_format, m_color_space, m_buffers
void Fl_Vk_Window_Driver::prepare_buffers() {
  VkResult result;
  VkSwapchainKHR oldSwapchain = pWindow->m_swapchain;
  pWindow->m_swapchain = VK_NULL_HANDLE;

  // Get surface capabilities
  VkSurfaceCapabilitiesKHR surfCapabilities;
  result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pWindow->m_gpu,
                                                     pWindow->m_surface,
                                                     &surfCapabilities);
  VK_CHECK_RESULT(result);

  // Set swapchain extent to match window size, clamped to capabilities
  VkExtent2D swapchainExtent = {(uint32_t)pWindow->w(), (uint32_t)pWindow->h()};
  swapchainExtent.width = CLAMP(swapchainExtent.width, surfCapabilities.minImageExtent.width,
                                surfCapabilities.maxImageExtent.width);
  swapchainExtent.height = CLAMP(swapchainExtent.height, surfCapabilities.minImageExtent.height,
                                 surfCapabilities.maxImageExtent.height);

  // Check if extent matches request; if not, delay recreation
  if (swapchainExtent.width != pWindow->w() || swapchainExtent.height != pWindow->h()) {
    pWindow->m_swapchain = oldSwapchain; // Restore old swapchain
    return;
  }

  VkSwapchainCreateInfoKHR swapchain = {};
  swapchain.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain.surface = pWindow->m_surface;
  swapchain.minImageCount = (pWindow->mode() & FL_SINGLE) ? 1 : 2;
  if (swapchain.minImageCount < surfCapabilities.minImageCount) {
    swapchain.minImageCount = surfCapabilities.minImageCount;
  }
  swapchain.imageFormat = pWindow->m_format;
  swapchain.imageColorSpace = pWindow->m_color_space;
  swapchain.imageExtent = swapchainExtent;
  swapchain.imageArrayLayers = 1;
  swapchain.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchain.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchain.preTransform = surfCapabilities.currentTransform;
  swapchain.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain.presentMode =
      (swapchain.minImageCount == 1) ? VK_PRESENT_MODE_IMMEDIATE_KHR : VK_PRESENT_MODE_FIFO_KHR;
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
  if (pWindow->m_swapchain == VK_NULL_HANDLE) {
    printf("swapchain creation failed and returned NULL HANDLE\n");
  }

  if (result != VK_SUCCESS && pWindow->mode() & FL_SINGLE) {
    printf("Single buffering failed (%d), falling back to double buffering\n", result);
    swapchain.minImageCount = 2;
    swapchain.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    result = vkCreateSwapchainKHR(pWindow->m_device, &swapchain, NULL, &pWindow->m_swapchain);
  }
  if (result != VK_SUCCESS) {
    pWindow->m_swapchain = oldSwapchain;
    return; // Exit early on failure
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
  pWindow->m_buffers = (Fl_Vk_SwapchainBuffer *)malloc(pWindow->m_swapchainImageCount *
                                                       sizeof(Fl_Vk_SwapchainBuffer));
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

// Uses m_depth, m_device
void Fl_Vk_Window_Driver::prepare_depth() {
  bool has_depth = pWindow->mode() & FL_DEPTH;
  bool has_stencil = pWindow->mode() & FL_STENCIL;

  if (!has_depth && !has_stencil) {
    pWindow->m_depth.view = VK_NULL_HANDLE; // Ensure no invalid reference
    pWindow->m_depth.image = VK_NULL_HANDLE;
    pWindow->m_depth.mem = VK_NULL_HANDLE;
    return;
  }

  VkFormat depth_format = VK_FORMAT_D16_UNORM;
  if (has_stencil)
    depth_format = VK_FORMAT_D24_UNORM_S8_UINT;
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
  pass = memory_type_from_properties(pWindow, mem_reqs.memoryTypeBits, 0, /* No requirements */
                                     &mem_alloc.memoryTypeIndex);
  assert(pass);

  /* allocate memory */
  result = vkAllocateMemory(pWindow->m_device, &mem_alloc, NULL, &pWindow->m_depth.mem);
  VK_CHECK_RESULT(result);

  /* bind memory */
  result = vkBindImageMemory(pWindow->m_device, pWindow->m_depth.image, pWindow->m_depth.mem, 0);
  VK_CHECK_RESULT(result);

  set_image_layout(pWindow->m_depth.image, VK_IMAGE_ASPECT_DEPTH_BIT,
                   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                   0);

  /* create image view */
  view.image = pWindow->m_depth.image;
  result = vkCreateImageView(pWindow->m_device, &view, NULL, &pWindow->m_depth.view);
  VK_CHECK_RESULT(result);
}


// Uses m_framebuffers, m_renderPass, m_swapChainImageCount
void Fl_Vk_Window_Driver::prepare_framebuffers() {
  VkImageView attachments[2];
  attachments[0] = VK_NULL_HANDLE;        // Color attachment
  attachments[1] = pWindow->m_depth.view; // Depth/stencil (optional)

  VkFramebufferCreateInfo fb_info = {};
  fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  fb_info.pNext = NULL;
  fb_info.renderPass = pWindow->m_renderPass;
  bool has_depth = pWindow->mode() & FL_DEPTH;
  bool has_stencil = pWindow->mode() & FL_STENCIL;
  fb_info.attachmentCount = (has_depth || has_stencil) ? 2 : 1;
  fb_info.pAttachments = attachments;
  fb_info.width = pWindow->w();
  fb_info.height = pWindow->h();
  fb_info.layers = 1;

  VkResult result;
  uint32_t i;

  pWindow->m_framebuffers =
      (VkFramebuffer *)VK_ALLOC(pWindow->m_swapchainImageCount * sizeof(VkFramebuffer));
  assert(pWindow->m_framebuffers);

  for (i = 0; i < pWindow->m_swapchainImageCount; i++) {
    attachments[0] = pWindow->m_buffers[i].view;
    result = vkCreateFramebuffer(pWindow->m_device, &fb_info, NULL, &pWindow->m_framebuffers[i]);
    VK_CHECK_RESULT(result);
  }
}


bool Fl_Vk_Window_Driver::check_device_extension_support(VkPhysicalDevice device) {
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       availableExtensions.data());

  std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

  for (const auto &extension : availableExtensions) {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

QueueFamilyIndices Fl_Vk_Window_Driver::find_queue_families(VkPhysicalDevice physicalDevice) {
  QueueFamilyIndices indices;

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

  int i = 0;
  for (const auto &queueFamily : queueFamilies) {
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphicsFamily = i;
    }

    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, pWindow->m_surface, &presentSupport);

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


void Fl_Vk_Window_Driver::init_device() {
    VkResult result;

    float queue_priorities[1] = {0.0};
    VkDeviceQueueCreateInfo queue = {};
    queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.pNext = NULL;
    queue.queueFamilyIndex = pWindow->m_queueFamilyIndex;
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


void Fl_Vk_Window_Driver::init_vk() {
    VkResult err;
    VkBool32 portability_enumeration = VK_FALSE;
    uint32_t i = 0;
    uint32_t required_extension_count = 0;
    uint32_t instance_extension_count = 0;
    uint32_t instance_layer_count = 0;
    uint32_t validation_layer_count = 0;
    pWindow->m_enabled_extension_count = 0;
    pWindow->m_enabled_layer_count = 0;

    const char *instance_validation_layers[] = {
        "VK_LAYER_KHRONOS_validation",
    };


    /* Look for validation layers *//* Look for validation layers */
    VkBool32 validation_found = 0;
    if (pWindow->m_validate) {
        err = vkEnumerateInstanceLayerProperties(&instance_layer_count, NULL);
        VK_CHECK_RESULT(err);

        if (instance_layer_count > 0) {
            VkLayerProperties *instance_layers =
                (VkLayerProperties *)VK_ALLOC(sizeof(VkLayerProperties) * instance_layer_count);
            err = vkEnumerateInstanceLayerProperties(&instance_layer_count, instance_layers);
            VK_CHECK_RESULT(err);

            validation_found =
                check_layers(VK_ARRAY_SIZE(instance_validation_layers),
                             instance_validation_layers,
                             instance_layer_count, instance_layers);
            if (validation_found) {
                pWindow->m_enabled_layer_count = VK_ARRAY_SIZE(instance_validation_layers);
                pWindow->m_enabled_layers[0] = "VK_LAYER_KHRONOS_validation";
                validation_layer_count = 1;
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
        required_extensions[i];
    assert(pWindow->m_enabled_extension_count < 64);
  }

  required_extensions = pWindow->get_required_extensions();
  for (i = 0; i < required_extensions.size(); i++) {
    pWindow->m_extension_names[pWindow->m_enabled_extension_count++] =
        required_extensions[i];
    assert(pWindow->m_enabled_extension_count < 64);
  }

  auto optional_extensions = pWindow->get_optional_extensions();
  for (i = 0; i < optional_extensions.size(); i++) {
      if (isExtensionSupported(optional_extensions[i]))
      {
          pWindow->m_extension_names[pWindow->m_enabled_extension_count++] =
              optional_extensions[i];
          assert(pWindow->m_enabled_extension_count < 64);
      }
      else
      {
          if (pWindow->m_validate)
          {
              std::cerr << "Optional Vulkan extension "
                        << optional_extensions[i]
                        << " not found." << std::endl;
          }
      }
  }
  
  err = vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count, NULL);
  assert(!err);

  if (instance_extension_count > 0) {
    VkExtensionProperties *instance_extensions =
        (VkExtensionProperties *)VK_ALLOC(sizeof(VkExtensionProperties) * instance_extension_count);
    err = vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count,
                                                 instance_extensions);
    assert(!err);
    for (i = 0; i < instance_extension_count; i++) {
      if (!strcmp(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, instance_extensions[i].extensionName)) {
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


  // Make initial call to query gpu_count, then second call for gpu info
  // Make initial call to query gpu_count
  uint32_t gpu_count = 0;
  err = vkEnumeratePhysicalDevices(pWindow->m_instance, &gpu_count, NULL);
  if (err != VK_SUCCESS || gpu_count == 0) {
      Fl::fatal("vkEnumeratePhysicalDevices failed to find any GPUs.\n\n"
                "Error code: %d\n"
                "Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
                "Please look at the Getting Started guide for additional information.");
}
  
// Allocate and populate physical devices array
  VkPhysicalDevice *physical_devices = (VkPhysicalDevice *)VK_ALLOC(sizeof(VkPhysicalDevice) * gpu_count);
  assert(physical_devices); // Ensure allocation succeeded
  err = vkEnumeratePhysicalDevices(pWindow->m_instance, &gpu_count, physical_devices);
  if (err != VK_SUCCESS) {
      VK_FREE(physical_devices);
      VK_CHECK_RESULT(err);
      Fl::fatal("vkEnumeratePhysicalDevices failed to enumerate devices.\n"
                "Error code");
  }

// Assign the first GPU handle and free the array
  pWindow->m_gpu = physical_devices[0];
  VK_FREE(physical_devices);

// Look for device extensions
  uint32_t device_extension_count = 0;
  VkBool32 swapchainExtFound = 0;
  pWindow->m_enabled_extension_count = 0;

  err = vkEnumerateDeviceExtensionProperties(pWindow->m_gpu, NULL,
                                             &device_extension_count, NULL);
  assert(!err);

  if (device_extension_count > 0) {
      VkExtensionProperties *device_extensions =
          (VkExtensionProperties *)VK_ALLOC(sizeof(VkExtensionProperties) * device_extension_count);
      err = vkEnumerateDeviceExtensionProperties(pWindow->m_gpu, NULL,
                                                 &device_extension_count,
                                                 device_extensions);
    assert(!err);

    for (i = 0; i < device_extension_count; i++) {
              
        if (!strcmp(VK_KHR_SWAPCHAIN_EXTENSION_NAME, device_extensions[i].extensionName)) {
            swapchainExtFound = 1;
            pWindow->m_extension_names[pWindow->m_enabled_extension_count++] =
                VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        }
        
#ifdef VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
        std::cerr << VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
                  << std::endl;
        FLTK_ADD_EXTENSION(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
        FLTK_ADD_EXTENSION(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

    }
          
    auto required_device_extensions = pWindow->get_device_extensions();
    
    for (unsigned j = 0; j < required_device_extensions.size(); ++j)
    {
        for (i = 0; i < device_extension_count; i++) {
            if (!strcmp(required_device_extensions[j],
                        device_extensions[i].extensionName)) {
                pWindow->m_extension_names[pWindow->m_enabled_extension_count++] =
                    required_device_extensions[j];
                std::cerr << "added " << required_device_extensions[j]
                          << std::endl;
            }

        }
               
    }

    VK_FREE(device_extensions);
  }

  if (!swapchainExtFound) {
    Fl::fatal("vkEnumerateDeviceExtensionProperties failed to find "
              "the " VK_KHR_SWAPCHAIN_EXTENSION_NAME " extension.\n\nDo you have a compatible "
              "Vulkan installable client driver (ICD) installed?\nPlease "
              "look at the Getting Started guide for additional "
              "information.\n",
              "vkCreateInstance Failure");
  }

  vkGetPhysicalDeviceProperties(pWindow->m_gpu, &pWindow->m_gpu_props);

  // Query with NULL data to get count
  vkGetPhysicalDeviceQueueFamilyProperties(pWindow->m_gpu, &pWindow->m_queue_count, NULL);

  pWindow->m_queue_props =
      (VkQueueFamilyProperties *)VK_ALLOC(pWindow->m_queue_count * sizeof(VkQueueFamilyProperties));
  vkGetPhysicalDeviceQueueFamilyProperties(pWindow->m_gpu,
                                           &pWindow->m_queue_count,
                                           pWindow->m_queue_props);
  assert(pWindow->m_queue_count >= 1);

  vkGetPhysicalDeviceFeatures(pWindow->m_gpu, &pWindow->m_gpu_features);

}

void Fl_Vk_Window_Driver::init_vk_swapchain() {
  VkResult result;
  uint32_t i;
  
  // Get Memory information and properties
  vkGetPhysicalDeviceMemoryProperties(pWindow->m_gpu, &pWindow->m_memory_properties);  // Iterate over each queue to learn whether it supports presenting:
  VkBool32 *supportsPresent = (VkBool32 *)VK_ALLOC(pWindow->m_queue_count * sizeof(VkBool32));
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

  // Generate result or if could not find both a graphics and a present queue
  if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX) {
    Fl::fatal("Could not find a graphics and a present queue\n",
              "Swapchain Initialization Failure");
  }

  // NOTE: While it is possible for an application to use a separate graphics
  //       and a present queues, this demo program assumes it is only using
  //       one.
  if (graphicsQueueNodeIndex != presentQueueNodeIndex) {
    Fl::fatal("Could not find a common graphics and a present queue\n",
              "Swapchain Initialization Failure");
  }

  pWindow->m_queueFamilyIndex = graphicsQueueNodeIndex;

  if (pWindow->m_device == VK_NULL_HANDLE)
      init_device();

  vkGetDeviceQueue(pWindow->m_device, pWindow->m_queueFamilyIndex, 0, &pWindow->m_queue);

  uint32_t formatCount;
  result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      pWindow->m_gpu, pWindow->m_surface, &formatCount, NULL);
  VK_CHECK_RESULT(result);

  std::vector<VkSurfaceFormatKHR> formats(formatCount);
  result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      pWindow->m_gpu, pWindow->m_surface, &formatCount, formats.data());
  VK_CHECK_RESULT(result);

  // Look for HDR10 or HLG if present
  bool hdrMonitorFound = false;
  VkFormat m_format;
  VkColorSpaceKHR m_color_space;
  std::vector<int> scores(formats.size());
  for (const auto& format : formats)
  {
      scores[i] = 0;
      if (pWindow->m_validate)
      {
          // std::cerr << "[" << i << "] format ="
          //           << string_VkFormat(format.format) << std::endl
          //           << "color space = "
          //           << string_VkColorSpaceKHR(format.colorSpace)
          //           << std::endl;
      }
      switch (format.colorSpace)
      {
      case VK_COLOR_SPACE_HDR10_ST2084_EXT:
          scores[i] += 5000;
          hdrMonitorFound = true;
          break;
      case VK_COLOR_SPACE_HDR10_HLG_EXT:
          scores[i] += 1000;
          hdrMonitorFound = true;
          break;
      case VK_COLOR_SPACE_DOLBYVISION_EXT:
          scores[i] += 10000;
          hdrMonitorFound = true;
          break;
      default:
          break;
      }

      switch (format.format)
      {
      case VK_FORMAT_UNDEFINED:
      case VK_FORMAT_R8G8B8_UNORM:
      case VK_FORMAT_B8G8R8_UNORM:
      case VK_FORMAT_R8G8B8A8_UNORM:
      case VK_FORMAT_B8G8R8A8_UNORM:
      case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
          break;

      case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
      case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
          scores[i] += 100;
          break;
          // Accept 16-bit formats for everything
      case VK_FORMAT_R16G16B16_UNORM:
      case VK_FORMAT_R16G16B16A16_UNORM:
          scores[i] += 200;
          break;
      default:
          break;
      }

      ++i;
  }

  if (!hdrMonitorFound)
  {
      std::cerr << "No HDR monitor found or configured for SDR!"
                << std::endl;
      bool foundLinear = false;
      for (const auto& format : formats)
      {
          // Prefer UNORM with SRGB_NONLINEAR (linear output intent)
          if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
              format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
          {
              m_format = VK_FORMAT_B8G8R8A8_UNORM;
              m_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
              foundLinear = true;
              break;
          }
      }
      if (!foundLinear)
      {
          // Fallback to first supported format (usually works)
          m_format = formats[0].format;
          m_color_space = formats[0].colorSpace;
          std::cerr << "No ideal linear format found, using fallback"
                    << std::endl;
      }
  }
  else
  {
      std::cout << "HDR monitor found" << std::endl;
      // Default clips and washed out colors
      int best_score = 0;
      for (unsigned i = 0; i < formats.size(); ++i)
      {
          if (scores[i] > best_score)
          {
              best_score = scores[i];
              m_format = formats[i].format;
              m_color_space = formats[i].colorSpace;
          }
      }
  }

  // Handle undefined format case
  if (formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
  {
      m_format = VK_FORMAT_B8G8R8A8_UNORM;
      m_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  }

  pWindow->m_format = m_format;
  pWindow->m_color_space = m_color_space;

  std::cout << "\tSelected format = " << string_VkFormat(m_format)
            << std::endl
            << "\tSelected color space = "
            << string_VkColorSpaceKHR(m_color_space) << std::endl;
}

void Fl_Vk_Window_Driver::prepare() {
  VkResult result;

  VkCommandPoolCreateInfo cmd_pool_info = {};
  cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cmd_pool_info.pNext = NULL;
  cmd_pool_info.queueFamilyIndex = pWindow->m_queueFamilyIndex;
  cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  result = vkCreateCommandPool(pWindow->m_device, &cmd_pool_info, NULL, &pWindow->m_cmd_pool);
  VK_CHECK_RESULT(result);


  VkCommandBufferAllocateInfo cmd = {};
  cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmd.pNext = NULL;
  cmd.commandPool = pWindow->m_cmd_pool;
  cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd.commandBufferCount = 1;

  result = vkAllocateCommandBuffers(pWindow->m_device, &cmd, &pWindow->m_draw_cmd);
  VK_CHECK_RESULT(result);

  prepare_buffers();
  if (pWindow->m_swapchain == VK_NULL_HANDLE) {
    printf("Swapchain recreation failed in resize\n");
    return; // Skip further setup if swapchain fails
  }
  prepare_depth();
  pWindow->prepare();
  prepare_framebuffers(); // can be kept in driver
}

// m_device, m_swapchainImageCount, m_framebuffers, m_desc_pool,
// m_setup_cmd, m_draw_cmd, m_cmd_pool, m_pipeline_layout, m_desc_layout,
// m_pipeline, m_renderPass, m_buffers, m_depth
void Fl_Vk_Window_Driver::destroy_resources() {
  if (!pWindow || !pWindow->m_device)
    return;

  uint32_t i;
  VkResult result;

  // Wait for all GPU operations to complete before destroying resources
  result = vkDeviceWaitIdle(pWindow->m_device);
  VK_CHECK_RESULT(result);

  // Destroy resources in reverse creation order (first, those of window)
  pWindow->destroy_resources();

  if (pWindow->m_buffers) {
    for (uint32_t i = 0; i < pWindow->m_swapchainImageCount; i++) {
      vkDestroyImageView(pWindow->m_device, pWindow->m_buffers[i].view, NULL);
    }
    free(pWindow->m_buffers);
    pWindow->m_buffers = NULL;
  }

  if (pWindow->m_depth.view != VK_NULL_HANDLE) {
    vkDestroyImageView(pWindow->m_device, pWindow->m_depth.view, NULL);
    pWindow->m_depth.view = VK_NULL_HANDLE;
  }

  if (pWindow->m_depth.image != VK_NULL_HANDLE) {
    vkDestroyImage(pWindow->m_device, pWindow->m_depth.image, NULL);
    pWindow->m_depth.image = VK_NULL_HANDLE;
  }

  if (pWindow->m_depth.mem != VK_NULL_HANDLE) {
    vkFreeMemory(pWindow->m_device, pWindow->m_depth.mem, NULL);
    pWindow->m_depth.mem = VK_NULL_HANDLE;
  }
}

SwapChainSupportDetails
Fl_Vk_Window_Driver::query_swap_chain_support(VkPhysicalDevice physicalDevice) {
  SwapChainSupportDetails details;

  // // Get Surface Capabilities
  // vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice,
  //                                           pWindow->m_surface,
  //                                           &details.capabilities);

  // // Get Surface Formats
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

  // // Get Present Modes
  // uint32_t presentModeCount;
  // vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice,
  //                                           pWindow->m_surface,
  //                                           &presentModeCount, nullptr);

  // if (presentModeCount != 0) {
  //     details.presentModes.resize(presentModeCount);
  //     vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice,
  //                                               pWindow->m_surface, &presentModeCount,
  //                                               details.presentModes.data());
  // }

  return details;
}


void Fl_Vk_Window_Driver::swap_buffers() {}
