#include <FL/Fl_Vk_Utils.H>

#include <stdexcept>

std::vector<uint32_t> compile_glsl_to_spirv(const std::string &source_code,
                                            shaderc_shader_kind shader_kind,
                                            const std::string &filename, bool optimize) {

  // Initialize the compiler
  shaderc::Compiler compiler;
  shaderc::CompileOptions options;

  // Optional: Set optimization level
  if (optimize) {
    options.SetOptimizationLevel(shaderc_optimization_level_performance);
  }

  // Set target environment to Vulkan 1.0 (adjust as needed)
  options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);

  // Compile the GLSL source
  shaderc::SpvCompilationResult result =
      compiler.CompileGlslToSpv(source_code,      // GLSL source code
                                shader_kind,      // Shader type (e.g., vertex, fragment)
                                filename.c_str(), // Name for error reporting
                                options           // Compilation options
      );

  // Check for compilation errors
  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    throw std::runtime_error("Shader compilation failed: " + std::string(result.GetErrorMessage()));
  }

  // Return the SPIR-V bytecode as a vector of uint32_t
  return std::vector<uint32_t>(result.cbegin(), result.cend());
}

// Example usage in your Vulkan code
VkShaderModule create_shader_module(VkDevice device, const std::vector<uint32_t> &spirv_code) {
  VkShaderModuleCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = spirv_code.size() * sizeof(uint32_t);
  create_info.pCode = spirv_code.data();

  VkShaderModule shader_module;
  VkResult result = vkCreateShaderModule(device, &create_info, nullptr, &shader_module);
  if (result != VK_SUCCESS) {
    std::cerr << "Shader module creation failed: " << result << std::endl;
    return VK_NULL_HANDLE;
  }
  return shader_module;
}

void set_image_layout(VkCommandBuffer cmd,
                      VkImage image,
                      VkImageAspectFlags aspectMask,
                      VkImageLayout old_image_layout,
                      VkImageLayout new_image_layout,
                      VkAccessFlags srcAccessMask,
                      VkPipelineStageFlags srcStageMask,
                      VkAccessFlags dstAccessMask,
                      VkPipelineStageFlags dstStageMask)
{
    VkResult err;

    VkImageMemoryBarrier image_memory_barrier = {};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.pNext = NULL;
    image_memory_barrier.srcAccessMask = srcAccessMask;
    image_memory_barrier.dstAccessMask = dstAccessMask;
    image_memory_barrier.oldLayout = old_image_layout;
    image_memory_barrier.newLayout = new_image_layout;
    image_memory_barrier.image = image;
    image_memory_barrier.subresourceRange = {aspectMask, 0, 1, 0, 1};

    vkCmdPipelineBarrier(cmd,
                         srcStageMask, dstStageMask, 0, 0, NULL,
                         0, NULL, 1, &image_memory_barrier);
}
