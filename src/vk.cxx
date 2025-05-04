

#include "FL/vk_enum_string_helper.h"
#include <vulkan/vulkan.h>


#include "FL/Fl.H"
#include "FL/vk.h"


void vk_check_result(VkResult err, const char* file, const int line)
{
    const char* errorName = string_VkResult(err);
    if (err == VK_SUCCESS)
        return;
    fprintf(stderr, "Vulkan: %s (%u) at %s, line %d", errorName, err, file, line);
#ifdef NDEBUG
    abort();
#endif
}
