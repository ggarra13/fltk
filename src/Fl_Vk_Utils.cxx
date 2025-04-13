#include <FL/Fl_Vk_Utils.H>

#include <iostream>
#include <stdexcept>

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

  // Set target environment to Vulkan 1.3 (adjust as needed)
  options.SetTargetEnvironment(shaderc_target_env_vulkan,
                               shaderc_env_version_vulkan_1_3);

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


