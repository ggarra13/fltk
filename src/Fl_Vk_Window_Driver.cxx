/*
 * Copyright (c) 2015-2016 The Khronos Group Inc.
 * Copyright (c) 2015-2016 Valve Corporation
 * Copyright (c) 2015-2016 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Chia-I Wu <olvaffe@gmail.com>
 * Author: Cody Northrop <cody@lunarg.com>
 * Author: Courtney Goeltzenleuchter <courtney@LunarG.com>
 * Author: Ian Elliott <ian@LunarG.com>
 * Author: Jon Ashburn <jon@lunarg.com>
 * Author: Piers Daniell <pdaniell@nvidia.com>
 * Author: Gwan-gyeong Mun <elongbug@gmail.com>
 * Author: Camilla Löwy <elmindreda@glfw.org>
 * Porter: Gonzalo Garramuño <ggarra13@gmail.com>
 *
 */
#include <FL/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#include "FL/Fl_Vk_Utils.H"
#include "Fl_Vk_Window_Driver.H"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

// macros
#define CLAMP(v, vmin, vmax) (v < vmin ? vmin : (v > vmax ? vmax : v))


//! Returns true or false if extension name is supported.
static bool isExtensionSupported(const char* extensionName) {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    for (const auto& ext : extensions)
    {
        if (strcmp(ext.extensionName, extensionName) == 0)
        {
            return true;
        }
    }
    return false;
}

/*
 * Return 1 (true) if all layer names specified in check_names
 * can be found in given layer properties.
 */
static VkBool32 check_layers(uint32_t check_count, const char **check_names,
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


// Uses m_swapchain, ctx.gpu, m_surface, m_format, m_buffers
void Fl_Vk_Window_Driver::prepare_buffers() {
  VkResult result;
  VkSwapchainKHR oldSwapchain = pWindow->m_swapchain;
  pWindow->m_swapchain = VK_NULL_HANDLE;

  // Get surface capabilities
  VkSurfaceCapabilitiesKHR surfCapabilities;
  result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pWindow->gpu(),
                                                     pWindow->m_surface,
                                                     &surfCapabilities);
  VK_CHECK(result);

  // Set swapchain extent to match window size, clamped to capabilities
  VkExtent2D swapchainExtent = {(uint32_t)pWindow->w(), (uint32_t)pWindow->h()};
  swapchainExtent.width = CLAMP(swapchainExtent.width, surfCapabilities.minImageExtent.width,
                                surfCapabilities.maxImageExtent.width);
  swapchainExtent.height = CLAMP(swapchainExtent.height, surfCapabilities.minImageExtent.height,
                                 surfCapabilities.maxImageExtent.height);

  // Skip recreation if extent matches current and old swapchain is valid
  if (oldSwapchain != VK_NULL_HANDLE && 
      swapchainExtent.width == pWindow->getCurrentExtent().width &&
      swapchainExtent.height == pWindow->getCurrentExtent().height) {
      pWindow->m_swapchain = oldSwapchain;
      return;
  }

  VkSwapchainCreateInfoKHR swapchain = {};
  swapchain.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain.surface = pWindow->m_surface;
  swapchain.minImageCount = (pWindow->mode() & FL_SINGLE) ? 1 : 2;
  if (swapchain.minImageCount < surfCapabilities.minImageCount) {
    swapchain.minImageCount = surfCapabilities.minImageCount;
  }
  swapchain.imageFormat = pWindow->format();
  swapchain.imageColorSpace = pWindow->colorSpace();
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
  for (auto& buffer : pWindow->m_buffers)
  {
      vkDestroyFramebuffer(pWindow->device(), buffer.framebuffer, NULL);
      vkDestroyImageView(pWindow->device(), buffer.view, NULL);
      // image belongs to the swapchain.
  }
  pWindow->m_buffers.clear();

  // Create new swapchain
  result = vkCreateSwapchainKHR(pWindow->device(), &swapchain, NULL, &pWindow->m_swapchain);
  VK_CHECK(result);
  if (pWindow->m_swapchain == VK_NULL_HANDLE)
  {
      fprintf(stderr, "vkCreateSwapchainKHR failed: %s\n", string_VkResult(result));
      pWindow->m_swapchain = oldSwapchain; // Restore old swapchain
      return;
  }

  if (oldSwapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(pWindow->device(), oldSwapchain, NULL);
  }

  // Get new swapchain images
  uint32_t swapchainImageCount = 0;
  result = vkGetSwapchainImagesKHR(pWindow->device(), pWindow->m_swapchain,
                                   &swapchainImageCount, NULL);
  VK_CHECK(result);

  std::vector<VkImage> swapchainImages(swapchainImageCount);
  result = vkGetSwapchainImagesKHR(pWindow->device(), pWindow->m_swapchain,
                                   &swapchainImageCount,
                                   swapchainImages.data());
  VK_CHECK(result);

  // Recreate buffers
  pWindow->m_buffers.resize(swapchainImageCount);
  for (uint32_t i = 0; i < swapchainImageCount; i++)
  {
      VkImageViewCreateInfo view_info = {};
      view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      view_info.image = swapchainImages[i];
      view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
      view_info.format = pWindow->format();
      view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      view_info.subresourceRange.levelCount = 1;
      view_info.subresourceRange.layerCount = 1;
      result = vkCreateImageView(pWindow->device(), &view_info, NULL,
                                 &pWindow->m_buffers[i].view);
      VK_CHECK(result);
      pWindow->m_buffers[i].image = swapchainImages[i];
  }
}

// Uses m_depth, ctx.device
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
  uint32_t aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

  if (has_stencil)
  {
      if (has_depth)
      {
          depth_format = VK_FORMAT_D24_UNORM_S8_UINT;
          aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      }
      else
      {
        depth_format = VK_FORMAT_S8_UINT;
        aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
      }
  }

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
  subresourceRange.aspectMask = aspectMask;
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
  result = vkCreateImage(pWindow->device(), &image, NULL,
                         &pWindow->m_depth.image);
  VK_CHECK(result);

  /* get memory requirements for this object */
  vkGetImageMemoryRequirements(pWindow->device(),
                               pWindow->m_depth.image, &mem_reqs);

  /* select memory size and type */
  mem_alloc.allocationSize = mem_reqs.size;
  pass = memory_type_from_properties(pWindow->gpu(), mem_reqs.memoryTypeBits, 0, /* No requirements */
                                     &mem_alloc.memoryTypeIndex);
  assert(pass);

  /* allocate memory */
  result = vkAllocateMemory(pWindow->device(), &mem_alloc, NULL,
                            &pWindow->m_depth.mem);
  VK_CHECK(result);

  /* bind memory */
  result = vkBindImageMemory(pWindow->device(), pWindow->m_depth.image, pWindow->m_depth.mem, 0);
  VK_CHECK(result);

  set_image_layout(pWindow->device(),
                   pWindow->commandPool(),
                   pWindow->queue(),
                   pWindow->m_depth.image,
                   aspectMask,
                   VK_IMAGE_LAYOUT_UNDEFINED,
                   VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                   0, VK_PIPELINE_STAGE_HOST_BIT,
                   0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

  /* create image view */
  view.image = pWindow->m_depth.image;
  result = vkCreateImageView(pWindow->device(), &view, NULL, &pWindow->m_depth.view);
  VK_CHECK(result);
}


// Uses m_renderPass
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
  for (auto& buffer : pWindow->m_buffers)
  {
      attachments[0] = buffer.view;
      result = vkCreateFramebuffer(pWindow->device(), &fb_info, NULL, &buffer.framebuffer);
      VK_CHECK(result);
  }
}

void Fl_Vk_Window_Driver::init_instance()
{
    VkResult err;
    VkBool32 portability_enumeration = VK_FALSE;
    uint32_t i = 0;

    const char *instance_validation_layers[] = {
        "VK_LAYER_KHRONOS_validation",
    };
    
    /* Look for validation layers *//* Look for validation layers */
    VkBool32 validation_found = 0;
    if (pWindow->m_validate) {
        uint32_t instance_layer_count = 0;
        err = vkEnumerateInstanceLayerProperties(&instance_layer_count, NULL);
        VK_CHECK(err);

        if (instance_layer_count > 0) {
            VkLayerProperties *instance_layers =
                (VkLayerProperties *)VK_ALLOC(sizeof(VkLayerProperties) * instance_layer_count);
            err = vkEnumerateInstanceLayerProperties(&instance_layer_count, instance_layers);
            VK_CHECK(err);

            validation_found =
                check_layers(VK_ARRAY_SIZE(instance_validation_layers),
                             instance_validation_layers,
                             instance_layer_count, instance_layers);
            if (validation_found) {
                pWindow->ctx.enabled_layers.push_back(instance_validation_layers[0]);
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
    {
        const std::vector<const char*>& instance_extensions =
            Fl_Vk_Window_Driver::driver(pWindow)->get_instance_extensions();
        if (instance_extensions.empty())
        {
            Fl::fatal("FLTK get_instance_extensions failed to find the "
                      "platform surface extensions.\n\nDo you have a compatible "
                      "Vulkan installable client driver (ICD) installed?\nPlease "
                      "look at the Getting Started guide for additional "
                      "information.\n",
                      "vkCreateInstance Failure");
        }
        

        for (const auto& extension : instance_extensions)
        {
            if (!isExtensionSupported(extension))
            {
                std::string unsupported = "Unsupported window driver extension '";
                unsupported += extension;
                unsupported += "'";
                Fl::fatal(unsupported.c_str());
            }
            pWindow->ctx.instance_extensions.push_back(extension);
        }
    }

    {
        const std::vector<const char*>& instance_extensions = pWindow->get_instance_extensions();
        for (const auto& extension : instance_extensions)
        {
            if (!isExtensionSupported(extension))
            {
                std::string unsupported = "Unsupported window extension '";
                unsupported += extension;
                unsupported += "'";
                Fl::fatal(unsupported.c_str());
            }
            pWindow->ctx.instance_extensions.push_back(extension);
        }
    }
    
    const std::vector<const char*>& optional_extensions =
        pWindow->get_optional_extensions();
    for (const auto& extension : optional_extensions)
    {
        if (isExtensionSupported(extension))
        {
            pWindow->ctx.instance_extensions.push_back(extension);
        }
        else
        {
            if (pWindow->m_validate)
            {
                fprintf(stderr, "Optional Vulkan extension '%s' not found.",
                        extension);
            }
        }
    }
  
    uint32_t instance_extension_count = 0;
    err = vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count, NULL);
    assert(!err);

    if (instance_extension_count > 0) {
        VkExtensionProperties *instance_extensions =
            (VkExtensionProperties *)VK_ALLOC(sizeof(VkExtensionProperties) *
                                              instance_extension_count);
        err = vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count,
                                                     instance_extensions);
        assert(!err);
        for (i = 0; i < instance_extension_count; i++)
        {
            if (!strcmp(VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
                        instance_extensions[i].extensionName))
            {
                if (pWindow->m_validate)
                {
                    pWindow->ctx.instance_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
                }
            }

            // This one is needed for MoltenVK
            if (!strcmp(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
                        instance_extensions[i].extensionName))
            {
                pWindow->ctx.instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
                portability_enumeration = VK_TRUE;
            }
        }

        free(instance_extensions);
    }

    VkApplicationInfo app = {};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pNext = NULL;
    app.pApplicationName = pWindow->application_name();
    app.applicationVersion = 0;
    app.pEngineName = pWindow->engine_name();
    app.engineVersion = 0;
    app.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo inst_info = {};
    inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_info.pNext = NULL;
    inst_info.pApplicationInfo = &app;
    inst_info.enabledLayerCount = pWindow->ctx.enabled_layers.size();
    inst_info.ppEnabledLayerNames = pWindow->ctx.enabled_layers.data();
    inst_info.enabledExtensionCount = pWindow->ctx.instance_extensions.size();
    inst_info.ppEnabledExtensionNames = pWindow->ctx.instance_extensions.data();

    if (portability_enumeration)
        inst_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    if (pWindow->m_instance == VK_NULL_HANDLE)
    {
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
    }
    
}

void Fl_Vk_Window_Driver::init_vk()
{
    VkResult err;
    uint32_t i = 0;
    
    if (pWindow->m_instance == VK_NULL_HANDLE)
        init_instance();
    
    pWindow->ctx.instance = pWindow->m_instance;

    // Make initial call to query gpu_count, then second call for gpu info
    // Make initial call to query gpu_count
    uint32_t gpu_count = 0;
    err = vkEnumeratePhysicalDevices(pWindow->ctx.instance, &gpu_count, NULL);
    if (err != VK_SUCCESS || gpu_count == 0)
    {
        Fl::fatal("vkEnumeratePhysicalDevices failed to find any GPUs.\n\n"
                  "Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
                  "Please look at the Getting Started guide for additional information.");
    }
  
    // Allocate and populate physical devices array
    VkPhysicalDevice *physicalDevices = (VkPhysicalDevice *)VK_ALLOC(sizeof(VkPhysicalDevice) * gpu_count);
    err = vkEnumeratePhysicalDevices(pWindow->ctx.instance, &gpu_count, physicalDevices);
    if (err != VK_SUCCESS) {
        VK_CHECK(err);
        VK_FREE(physicalDevices);
        Fl::fatal("vkEnumeratePhysicalDevices failed to enumerate devices.\n"
                  "Error code");
    }

    // Assign the first GPU handle and free the array
    pWindow->gpu() = physicalDevices[0];
    VK_FREE(physicalDevices);

    // Look for device extensions
    uint32_t device_extension_count = 0;
    VkBool32 swapchainExtFound = 0;

    err = vkEnumerateDeviceExtensionProperties(pWindow->gpu(), NULL,
                                               &device_extension_count, NULL);
    assert(!err);

#define FLTK_ADD_DEVICE_EXTENSION(x)                        \
    if (!strcmp(x, device_extensions[i].extensionName)) {   \
        pWindow->ctx.device_extensions.push_back(x);        \
    }

#ifndef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#   define VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME "VK_KHR_portability_subset"
#endif

#ifndef VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
#   define VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME "VK_KHR_get_physical_device_properties2"
#endif
  
    if (device_extension_count > 0)
    {
        VkExtensionProperties *device_extensions =
            (VkExtensionProperties *)VK_ALLOC(sizeof(VkExtensionProperties) * device_extension_count);
        err = vkEnumerateDeviceExtensionProperties(pWindow->gpu(), NULL,
                                                   &device_extension_count,
                                                   device_extensions);
        assert(!err);
      
        for (i = 0; i < device_extension_count; i++)
        {
            if (!strcmp(VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                        device_extensions[i].extensionName))
            {
                swapchainExtFound = 1;
                pWindow->ctx.device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            }
        
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
            FLTK_ADD_DEVICE_EXTENSION(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
            FLTK_ADD_DEVICE_EXTENSION(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif
        }
          
        auto wanted_device_extensions = pWindow->get_device_extensions();
        for (const auto& extension : wanted_device_extensions)
        {
            for (i = 0; i < device_extension_count; i++)
            {
                if (!strcmp(extension,
                            device_extensions[i].extensionName))
                {
                    pWindow->ctx.device_extensions.push_back(extension);
                    break;
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

    vkGetPhysicalDeviceProperties(pWindow->gpu(), &pWindow->ctx.gpu_props);

    // Query with NULL data to get count
    vkGetPhysicalDeviceQueueFamilyProperties(pWindow->gpu(),
                                             &pWindow->ctx.queue_count, NULL);

    pWindow->ctx.queue_props =
        (VkQueueFamilyProperties *)VK_ALLOC(pWindow->ctx.queue_count *
                                            sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(pWindow->gpu(),
                                             &pWindow->ctx.queue_count,
                                             pWindow->ctx.queue_props);
    assert(pWindow->ctx.queue_count >= 1);

    vkGetPhysicalDeviceFeatures(pWindow->gpu(), &pWindow->ctx.gpu_features);

}

void Fl_Vk_Window_Driver::init_vk_swapchain() {
    VkResult result;
    uint32_t i;

    // Get Memory information and properties
    size_t numQueues = pWindow->ctx.queue_count;
    vkGetPhysicalDeviceMemoryProperties(pWindow->gpu(),
                                        &pWindow->m_memory_properties);

    // Iterate over each queue to learn whether it supports presenting:
    VkBool32 *supportsPresent = (VkBool32 *)VK_ALLOC(numQueues *
                                                     sizeof(VkBool32));
    for (i = 0; i < numQueues; i++) {
        vkGetPhysicalDeviceSurfaceSupportKHR(pWindow->gpu(), i,
                                             pWindow->m_surface,
                                         &supportsPresent[i]);
  }

  // Search for a graphics and a present queue in the array of queue
  // families, try to find one that supports both
  uint32_t graphicsQueueNodeIndex = UINT32_MAX;
  uint32_t presentQueueNodeIndex = UINT32_MAX;
  for (i = 0; i < numQueues; i++) {
    if ((pWindow->ctx.queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
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
    for (i = 0; i < numQueues; ++i) {
      if (supportsPresent[i] == VK_TRUE) {
        presentQueueNodeIndex = i;
        break;
      }
    }
  }
  free(supportsPresent);

  // Generate result or if could not find both a graphics and a present queue
  if (graphicsQueueNodeIndex == UINT32_MAX ||
      presentQueueNodeIndex == UINT32_MAX) {
    Fl::fatal("Could not find a graphics and a present queue\n",
              "Swapchain Initialization Failure");
  }

  // \@note: It is possible for an application to use a separate graphics
  //         and a present queues.  Here we assume not.
  if (graphicsQueueNodeIndex != presentQueueNodeIndex) {
    Fl::fatal("Could not find a common graphics and a present queue\n",
              "Swapchain Initialization Failure");
  }

  pWindow->m_queueFamilyIndex = graphicsQueueNodeIndex;
  
  if (pWindow->device() == VK_NULL_HANDLE)
      pWindow->create_device();

  vkGetDeviceQueue(pWindow->device(),
                   pWindow->m_queueFamilyIndex, 0,
                   &pWindow->ctx.queue);

  uint32_t formatCount;
  result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      pWindow->gpu(), pWindow->m_surface, &formatCount, NULL);
  VK_CHECK(result);

  std::vector<VkSurfaceFormatKHR> formats(formatCount);
  result = vkGetPhysicalDeviceSurfaceFormatsKHR(
      pWindow->gpu(), pWindow->m_surface, &formatCount, formats.data());
  VK_CHECK(result);

  // Look for HDR10 or HLG if present
  bool hdrMonitorFound = false;
  VkColorSpaceKHR color_space;
  std::vector<int> scores(formats.size());
  for (const auto& format : formats)
  {
      scores[i] = 0;
      if (pWindow->log_level() > 5)
      {
          printf("[%d] format = %s color space = %s\n",
                 i, string_VkFormat(format.format),
                 string_VkColorSpaceKHR(format.colorSpace));
      }
      switch (format.colorSpace)
      {
      case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT:
          scores[i] += 4000;
          hdrMonitorFound = true;
          break;
      case VK_COLOR_SPACE_HDR10_ST2084_EXT:
          scores[i] += 3000;
          hdrMonitorFound = true;
          break;
      case VK_COLOR_SPACE_HDR10_HLG_EXT:
          scores[i] += 2000;
          hdrMonitorFound = true;
          break;
      case VK_COLOR_SPACE_DOLBYVISION_EXT:
          scores[i] += 1000;
          hdrMonitorFound = true;
          break;
      case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
          scores[i] += 500; // SDR baseline
          break;
      default:
          break;
      }

      switch (format.format)
      {
          // Accept 16-bit formats for everything
      case VK_FORMAT_R16G16B16_UNORM:
      case VK_FORMAT_R16G16B16A16_UNORM:
          scores[i] += 2000;
          break;
      case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
      case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
          scores[i] += 1500;
          break;
      case VK_FORMAT_R8G8B8A8_UNORM:
      case VK_FORMAT_B8G8R8A8_UNORM:
      case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
          scores[i] += 500;
          break;
      default:
          break;
      }

      ++i;
  }

  VkFormat view_format;
  if (!hdrMonitorFound)
  {
      bool foundLinear = false;
      for (const auto& format : formats)
      {
          // Prefer UNORM with SRGB_NONLINEAR (linear output intent)
          if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
              format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
          {
              view_format = VK_FORMAT_B8G8R8A8_UNORM;
              color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
              foundLinear = true;
              break;
          }
      }
      if (!foundLinear)
      {
          // Fallback to first supported format (usually works)
          view_format = formats[0].format;
          color_space = formats[0].colorSpace;
          if (pWindow->log_level() > 2)
          {
              fprintf(stderr, "No ideal linear format found, using fallback\n");
          }
      }
  }
  else
  {
      // Default clips and washed out colors
      int best_score = 0;
      for (unsigned i = 0; i < formats.size(); ++i)
      {
          if (scores[i] > best_score)
          {
              best_score = scores[i];
              view_format = formats[i].format;
              color_space = formats[i].colorSpace;
          }
      }
  }

  // Handle undefined format case
  if (formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
  {
      view_format = VK_FORMAT_B8G8R8A8_UNORM;
      color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  }

  pWindow->ctx.format = view_format;
  pWindow->ctx.colorSpace = color_space;

  if (pWindow->log_level() > 2)
  {
      printf("%p\tSelected window format = %s\n"
             "%p\tSelected window color space = %s\n",
             pWindow, string_VkFormat(view_format),
             pWindow, string_VkColorSpaceKHR(color_space));
  }
}

void Fl_Vk_Window_Driver::prepare() {
  VkResult result;

  VkCommandPoolCreateInfo cmd_pool_info = {};
  cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cmd_pool_info.pNext = NULL;
  cmd_pool_info.queueFamilyIndex = pWindow->m_queueFamilyIndex;
  cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  result = vkCreateCommandPool(pWindow->device(), &cmd_pool_info, NULL,
                               &pWindow->commandPool());
  VK_CHECK(result);


  VkCommandBufferAllocateInfo cmd = {};
  cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmd.pNext = NULL;
  cmd.commandPool = pWindow->commandPool();
  cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd.commandBufferCount = 1;

  result = vkAllocateCommandBuffers(pWindow->device(), &cmd, &pWindow->m_draw_cmd);
  VK_CHECK(result);

  prepare_buffers();
  if (pWindow->m_swapchain == VK_NULL_HANDLE) {
    printf("Swapchain recreation failed in resize\n");
    return; // Skip further setup if swapchain fails
  }
  prepare_depth();
  pWindow->prepare();
  prepare_framebuffers(); // can be kept in driver
}

// Uses: ctx.device, m_framebuffers, m_buffers, m_depth
void Fl_Vk_Window_Driver::destroy_resources() {
  if (!pWindow || !pWindow->device())
    return;

  uint32_t i;
  VkResult result;

  // Destroy resources in reverse creation order (first, those of window)
  pWindow->destroy_resources();

  for (auto& buffer : pWindow->m_buffers)
  {
      vkDestroyFramebuffer(pWindow->device(), buffer.framebuffer, NULL);
      vkDestroyImageView(pWindow->device(), buffer.view, NULL);
      // image belongs to the swapchain.
  }
  pWindow->m_buffers.clear();
  
  pWindow->m_depth.destroy(pWindow->device());
}

