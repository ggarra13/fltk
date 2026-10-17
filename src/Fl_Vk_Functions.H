
#pragma once

#define FLTK_VK_FUN(name) PFN_vk##name name

struct FLTK_Vk_Functions
{
    PL_VK_FUN(QueueSubmit2KHR);
    PL_VK_FUN(SetHdrMetadataEXT);
};


#define PL_VK_DEV_FUN(N)                                        \
    {                                                           \
         .name = "vk" #N,                                       \
       .offset = offsetof(struct FLTK_Vk_Functions, N),         \
 .device_level = true,                                          \
   }
