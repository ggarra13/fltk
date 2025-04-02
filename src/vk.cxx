

#ifdef __linux__

#include "FL/vk_enum_string_helper.h"

#else

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#endif


#include "FL/Fl.H"
#include "FL/vk.h"


void vk_check_result(VkResult err, const char* file, const int line)
{
    const char* errorName = string_VkResult(err);
    if (err == VK_SUCCESS)
        return;
    char buf[256];
    snprintf(buf, 256, "Vulkan: %s (%u) at %s) line %d",
        errorName, err, file, line);
    Fl::fatal(buf);
}
