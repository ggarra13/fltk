

#include "FL/vk_enum_string_helper.h"
#include <vulkan/vulkan.h>


#include "FL/Fl.H"
#include "FL/vk.h"

#include <cstdlib>  // for abort


void vk_check_result(VkResult err, const char* file, const int line)
{
    const char* errorName = string_VkResult(err);
    if (err == VK_SUCCESS)
        return;
    fprintf(stderr, "Vulkan: %s (%u) at %s, line %d\n", errorName, err, file, line);
#ifndef NDEBUG
    abort();
#endif
}


void vk_set_object_name(VkDevice device,
                        VkObjectType object_type,
                        uint64_t object_handle,
                        const char* name)
{
    if (!fltk_vkSetDebugUtilsObjectNameEXT) {
        return;  // Function not available
    }
    
    VkDebugUtilsObjectNameInfoEXT name_info{};
    name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    name_info.objectType = object_type;
    name_info.objectHandle = object_handle;
    name_info.pObjectName = name;

    fltk_vkSetDebugUtilsObjectNameEXT(device, &name_info);
}
