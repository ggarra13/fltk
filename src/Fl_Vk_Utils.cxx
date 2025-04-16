#include <FL/Fl_Vk_Utils.H>

#include <iostream>
#include <stdexcept>
#include <cstring>

std::vector<uint32_t> compile_glsl_to_spirv(const std::string &source_code,
                                            shaderc_shader_kind shader_kind,
                                            const std::string &filename,
                                            bool optimize)
{

  // Initialize the compiler
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;

  // Optional: Set optimization level
  if (optimize)
  {
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
  }

  // Set target environment to Vulkan 1.2 (as that's the maximum on macOS)
  options.SetTargetEnvironment(shaderc_target_env_vulkan,
                               shaderc_env_version_vulkan_1_2);

  // Compile the GLSL source
  shaderc::SpvCompilationResult result =
      compiler.CompileGlslToSpv(source_code,      // GLSL source code
                                shader_kind,      // Shader type (e.g., vertex, fragment)
                                filename.c_str(), // Name for error reporting
                                options           // Compilation options
      );

  // Check for compilation errors
  if (result.GetCompilationStatus() != shaderc_compilation_status_success)
  {
    throw std::runtime_error("Shader compilation failed: " + std::string(result.GetErrorMessage()));
  }

  // Return the SPIR-V bytecode as a vector of uint32_t
  return std::vector<uint32_t>(result.cbegin(), result.cend());
}

// Example usage in your Vulkan code
VkShaderModule create_shader_module(VkDevice device,
                                    const std::vector<uint32_t> &spirv_code)
{
  VkShaderModuleCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = spirv_code.size() * sizeof(uint32_t);
  create_info.pCode = spirv_code.data();

  VkShaderModule shader_module;
  VkResult result = vkCreateShaderModule(device, &create_info, nullptr, &shader_module);
  if (result != VK_SUCCESS)
  {
    std::cerr << "Shader module creation failed: " << result << std::endl;
    return VK_NULL_HANDLE;
  }
  return shader_module;
}


VkCommandBuffer beginSingleTimeCommands(VkDevice device,
                                        VkCommandPool commandPool)
{
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void endSingleTimeCommands(VkCommandBuffer commandBuffer,
                           VkDevice device,
                           VkCommandPool commandPool,
                           VkQueue queue)
{
    vkEndCommandBuffer(commandBuffer);
    
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void set_image_layout(VkDevice device,
                      VkCommandPool commandPool,
                      VkQueue queue,
                      VkImage image,
                      VkImageAspectFlags aspectMask,
                      VkImageLayout oldLayout,
                      VkImageLayout newLayout,
                      VkAccessFlags srcAccessMask,
                      VkPipelineStageFlags srcStageMask,
                      VkAccessFlags dstAccessMask,
                      VkPipelineStageFlags dstStageMask)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(device,
                                                            commandPool);
    
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext = NULL;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.image = image;
    barrier.subresourceRange = {aspectMask, 0, 1, 0, 1};

    vkCmdPipelineBarrier(commandBuffer,
                         srcStageMask, dstStageMask, 0, 0, NULL,
                         0, NULL, 1, &barrier);
    
    endSingleTimeCommands(commandBuffer, device, commandPool, queue);
}

FL_EXPORT void transitionImageLayout(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue queue,
    VkImage image,
    VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;
    VkAccessFlags srcAccessMask, dstAccessMask;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        srcAccessMask = 0;
        dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (
        oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
        newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        throw std::runtime_error("Unsupported layout transition");
    }
    
    set_image_layout(device,
                     commandPool,
                     queue,
                     image,
                     VK_IMAGE_ASPECT_COLOR_BIT,
                     oldLayout,
                     newLayout,
                     srcAccessMask,
                     sourceStage,
                     dstAccessMask,
                     destinationStage);
}



VkImageView createImageView(VkDevice device,
                            VkImage image, VkFormat format,
                            VkImageType imageType)
{
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType =
        (imageType == VK_IMAGE_TYPE_1D)   ? VK_IMAGE_VIEW_TYPE_1D
        : (imageType == VK_IMAGE_TYPE_2D) ? VK_IMAGE_VIEW_TYPE_2D
        : VK_IMAGE_VIEW_TYPE_3D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create image view");
    }
    return imageView;
}

VkImage createImage(
    VkDevice device,
    VkImageType imageType, uint32_t width, uint32_t height, uint32_t depth,
    VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage)
{

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = imageType;
    imageInfo.format = format;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = depth;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = tiling;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image;
    if (vkCreateImage(device, &imageInfo, nullptr, &image) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create image");
    }
    return image;
}

bool memory_type_from_properties(VkPhysicalDevice gpu,
                                 uint32_t typeBits, VkFlags requirements_mask,
                                 uint32_t *typeIndex)
{
  uint32_t i;

  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(gpu, &memProperties);
    
  // Search memtypes to find first index with those properties
  for (i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
    if ((typeBits & 1) == 1) {
      // Type is available, does it match user properties?
      if ((memProperties.memoryTypes[i].propertyFlags & requirements_mask) ==
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

VkDeviceMemory allocateAndBindImageMemory(VkDevice device,
                                          VkPhysicalDevice gpu,
                                          VkImage image,
                                          VkFlags requirementsMask)
{
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);


    uint32_t memoryTypeIndex = 0;
    bool ok = memory_type_from_properties(gpu, memRequirements.memoryTypeBits,
                                          requirementsMask, &memoryTypeIndex);
    if (!ok)
    {
        throw std::runtime_error("No memory types matched");
    }
    // for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
    // {
    //     if ((memRequirements.memoryTypeBits & (1 << i)) &&
    //         (memProperties.memoryTypes[i].propertyFlags &
    //          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
    //     {
    //         memoryTypeIndex = i;
    //         break;
    //     }
    // }

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    VkDeviceMemory imageMemory;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate image memory");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
    return imageMemory;
}

FL_EXPORT void createBuffer(
    VkDevice device,
    VkPhysicalDevice gpu,
    VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, VkBuffer& buffer,
    VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(gpu, &memProperties);

    uint32_t memoryTypeIndex = 0;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
    {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) ==
            properties)
        {
            memoryTypeIndex = i;
            break;
        }
    }

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

VkSampler createSampler(VkDevice device,
                        VkFilter magFilter,
                        VkFilter minFilter,
                        VkSamplerAddressMode addressModeU,
                        VkSamplerAddressMode addressModeV,
                        VkSamplerAddressMode addressModeW)
{
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = magFilter;
    samplerInfo.minFilter = minFilter;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = addressModeU;
    samplerInfo.addressModeV = addressModeV;
    samplerInfo.addressModeW = addressModeW;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    VkSampler sampler;
    if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) !=
        VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create sampler");
    }
    return sampler;
}

void uploadTextureData(VkDevice device,
                       VkPhysicalDevice gpu,
                       VkCommandPool commandPool,
                       VkQueue queue,
                       VkImage image,
                       uint32_t width,
                       uint32_t height,
                       uint32_t depth,
                       VkFormat format,
                       const uint16_t channels,
                       const uint16_t pixel_fmt_size,
                       const void* data)
{
    VkResult result;

    VkDeviceSize imageSize = width * height * depth * channels * pixel_fmt_size;

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    createBuffer(
        device, gpu,
        imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingBufferMemory);

    // Copy data to staging buffer
    void* mappedData;
    result = vkMapMemory(
        device, stagingBufferMemory, 0, imageSize, 0, &mappedData);
    VK_CHECK(result);

    std::memcpy(mappedData, data, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    // Copy staging buffer to image
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, depth};

    vkCmdCopyBufferToImage(
        commandBuffer, stagingBuffer, image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer, device,
                          commandPool, queue);

    // Clean up staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}
