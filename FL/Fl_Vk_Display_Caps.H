// Fl_Vk_Display_Caps.H
//
// Tiny, dependency-free description of what the *target display* can show,
// so the FLTK Vulkan layer can pick a (VkFormat, VkColorSpaceKHR) pair without
// knowing anything about tlRender / mrv2 monitor types.
//
// Include this from Fl_Vk_Window.H.

#ifndef Fl_Vk_Display_Caps_H
#define Fl_Vk_Display_Caps_H

#include <vulkan/vulkan.h>

//! What the display can show right now.
struct Fl_Vk_Display_Caps {
    //! Can show more than SDR range (EDR headroom on macOS, OS HDR enabled on
    //! Windows).  Gates HDR10 / HLG / scRGB / Dolby Vision.
    bool hdr = false;

    //! Display P3 (or wider) content is shown correctly.  This is true on
    //! every Mac, whatever display is attached, because the compositor
    //! color-manages the layer's color space to the display's profile.
    bool wide_gamut = false;
};

//! Target for a headless (offscreen) window.  There is no display to query,
//! so the application says what it wants the frames to be.
enum class Fl_Vk_Output_Profile {
    SDR,        // 8-bit sRGB (default: what PNG / video encoders expect)
    WideGamut,  // Display P3, 10-bit when possible
    HDR10,      // PQ / Rec.2020, 10-bit
    ScRGB       // linear extended sRGB, float16
};

inline Fl_Vk_Display_Caps fl_vk_caps_for_profile(Fl_Vk_Output_Profile p)
{
    Fl_Vk_Display_Caps c;
    c.hdr        = (p == Fl_Vk_Output_Profile::HDR10 ||
                    p == Fl_Vk_Output_Profile::ScRGB);
    c.wide_gamut = (p != Fl_Vk_Output_Profile::SDR);
    return c;
}

inline bool fl_vk_is_hdr_color_space(VkColorSpaceKHR cs)
{
    switch (cs) {
    case VK_COLOR_SPACE_HDR10_ST2084_EXT:
    case VK_COLOR_SPACE_HDR10_HLG_EXT:
    case VK_COLOR_SPACE_DOLBYVISION_EXT:
    case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
        return true;
    default:
        return false;
    }
}

#endif
