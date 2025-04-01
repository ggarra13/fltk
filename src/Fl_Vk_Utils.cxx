#include <FL/Fl_Vk_Utils.H>

#include <iostream>
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
                      VkImageLayout oldLayout,
                      VkImageLayout newLayout,
                      VkAccessFlags srcAccessMask,
                      VkPipelineStageFlags srcStageMask,
                      VkAccessFlags dstAccessMask,
                      VkPipelineStageFlags dstStageMask)
{
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext = NULL;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.image = image;
    barrier.subresourceRange = {aspectMask, 0, 1, 0, 1};

    vkCmdPipelineBarrier(cmd,
                         srcStageMask, dstStageMask, 0, 0, NULL,
                         0, NULL, 1, &barrier);
}

std::string getVkFormat(const VkFormat format)
{
    return "Unknown swapchain surace format";
}

std::string getVkSurfaceFormat(const VkSurfaceFormatKHR surfFormat)
{
    return "Unknown surace format";
}


#define VK_CHECK_COLOR_SPACE(x) case x: \
    out = #x; \
    break;

std::string getVkColorSpace(const VkColorSpaceKHR colorSpace)
{
    std::string out;
    switch(colorSpace)
    {
        VK_CHECK_COLOR_SPACE(VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
        VK_CHECK_COLOR_SPACE(VK_COLOR_SPACE_BT709_NONLINEAR_EXT);
        VK_CHECK_COLOR_SPACE(VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT);
        VK_CHECK_COLOR_SPACE(VK_COLOR_SPACE_DCI_P3_LINEAR_EXT);
        VK_CHECK_COLOR_SPACE(VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT);
        VK_CHECK_COLOR_SPACE(VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT);
        VK_CHECK_COLOR_SPACE(VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT);
        VK_CHECK_COLOR_SPACE(VK_COLOR_SPACE_BT709_LINEAR_EXT);
        VK_CHECK_COLOR_SPACE(VK_COLOR_SPACE_BT2020_LINEAR_EXT);
        VK_CHECK_COLOR_SPACE(VK_COLOR_SPACE_HDR10_ST2084_EXT);
        VK_CHECK_COLOR_SPACE(VK_COLOR_SPACE_DOLBYVISION_EXT);
        VK_CHECK_COLOR_SPACE(VK_COLOR_SPACE_HDR10_HLG_EXT);
        VK_CHECK_COLOR_SPACE(VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT);
        VK_CHECK_COLOR_SPACE(VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT);
        VK_CHECK_COLOR_SPACE(VK_COLOR_SPACE_PASS_THROUGH_EXT);
#ifdef VK_AMD_display_native_hdr
        VK_CHECK_COLOR_SPACE(VK_COLOR_SPACE_DISPLAY_NATIVE_AMD);
#endif

    default:
        out = "Unknown VK_COLOR_SPACE constant";
        break;
    }
    return out;
}
