#include <FL/Fl_Vk_Utils.H>

#include <stdexcept>

std::vector<uint32_t> compile_glsl_to_spirv(
    const std::string& source_code,
    shaderc_shader_kind shader_kind,
    const std::string& filename,
    bool optimize)
{

    // Initialize the compiler
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    // Optional: Set optimization level
    if (optimize) {
        options.SetOptimizationLevel(shaderc_optimization_level_performance);
    }

    // Set target environment to Vulkan 1.0 (adjust as needed)
    options.SetTargetEnvironment(shaderc_target_env_vulkan,
                                 shaderc_env_version_vulkan_1_0);

    // Compile the GLSL source
    shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(
        source_code,             // GLSL source code
        shader_kind,             // Shader type (e.g., vertex, fragment)
        filename.c_str(),        // Name for error reporting
        options                  // Compilation options
    );

    // Check for compilation errors
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        throw std::runtime_error("Shader compilation failed: " + std::string(result.GetErrorMessage()));
    }

    // Return the SPIR-V bytecode as a vector of uint32_t
    return std::vector<uint32_t>(result.cbegin(), result.cend());
}

// Example usage in your Vulkan code
VkShaderModule create_shader_module(VkDevice device,
                                    const std::vector<uint32_t>& spirv_code) {
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
