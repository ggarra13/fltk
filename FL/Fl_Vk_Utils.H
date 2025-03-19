#ifndef FL_Vk_Utils_H
#define FL_Vk_Utils_H

#include <shaderc/shaderc.hpp>

#include <string>
#include <vector>

// Function used to compile a glsl shader to SPRIV (needs shaderc)
std::vector<uint32_t> compile_glsl_to_spirv(
    const std::string& source_code,
    shaderc_shader_kind shader_kind,
    const std::string& filename = "shader.glsl",
    bool optimize = false);


#endif
