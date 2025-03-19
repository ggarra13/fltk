#ifndef FL_Vk_Utils_H
#define FL_Vk_Utils_H

#include <FL/vk.h>
#include <shaderc/shaderc.hpp>

#include <string>
#include <vector>

// Function used to compile a glsl shader to SPRIV (needs shaderc)
std::vector<uint32_t> compile_glsl_to_spirv(
    const std::string& source_code,
    shaderc_shader_kind shader_kind,
    const std::string& filename = "shader.glsl",
    bool optimize = false);

VkShaderModule create_shader_module(VkDevice device,
                                    const std::vector<uint32_t>& spirv_code);

#endif
