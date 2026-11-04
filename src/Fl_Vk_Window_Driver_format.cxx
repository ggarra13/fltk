// ---------------------------------------------------------------------------
// Fl_Vk_Window_Driver: surface / offscreen format selection
//
// Replaces everything in Fl_Vk_Window_Driver::init_colorspace() from
//
//     VkColorSpaceKHR color_space;
//     VkFormat view_format;
//
// down to the closing brace, with a single call:
//
//     select_surface_format();
//
// init_colorspace() then only does what its name does not say: queue family
// selection, device + VMA creation and ctx population (run once).
// select_surface_format() is cheap and can be re-run whenever the window
// moves to another screen (see Fl_Vk_Window::reselect_surface_format()).
//
// Header additions (Fl_Vk_Window_Driver.H):
//     void select_surface_format();
//   private:
//     std::vector<VkSurfaceFormatKHR> query_surface_formats();
//     std::vector<VkSurfaceFormatKHR> headless_candidates(Fl_Vk_Output_Profile);
// ---------------------------------------------------------------------------

#include "FL/Fl_Vk_Display_Caps.H"
#include "FL/vk_enum_string_helper.h"
#include "Fl_Vk_Window_Driver.H"

#include <vector>

namespace {

// Returns -1 for (format, color space) pairs that must never be chosen.
int score_pair(const VkSurfaceFormatKHR& sf, const Fl_Vk_Display_Caps& caps)
{
    const VkFormat fmt = sf.format;

    const bool is16f = (fmt == VK_FORMAT_R16G16B16A16_SFLOAT);
    const bool is16u = (fmt == VK_FORMAT_R16G16B16A16_UNORM);
    const bool is10  = (fmt == VK_FORMAT_A2R10G10B10_UNORM_PACK32 ||
                        fmt == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
    const bool is8   = (fmt == VK_FORMAT_R8G8B8A8_UNORM ||
                        fmt == VK_FORMAT_B8G8R8A8_UNORM ||
                        fmt == VK_FORMAT_A8B8G8R8_UNORM_PACK32);

    int cs = 0;
    switch (sf.colorSpace) {
    case VK_COLOR_SPACE_HDR10_ST2084_EXT:
        if (!caps.hdr || !(is10 || is16f || is16u)) return -1;
        cs = 4000;
        break;
    case VK_COLOR_SPACE_HDR10_HLG_EXT:
        if (!caps.hdr || !(is10 || is16f || is16u)) return -1;
        cs = 3000;
        break;
    case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
        // scRGB is only meaningful with float16 (it needs values < 0 and > 1).
        if (!caps.hdr || !is16f) return -1;
        cs = 1500;
        break;
    case VK_COLOR_SPACE_DOLBYVISION_EXT:
        //! @todo: Dolby Vision isn't handled yet, so it scores low.
        if (!caps.hdr) return -1;
        cs = 1000;
        break;
    case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT:
        // Wide gamut, NOT HDR.  Safe on any Mac display: the compositor
        // converts to the display profile.
        cs = caps.wide_gamut ? 2000 : 0;
        break;
    case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
        cs = 500;   // SDR baseline
        break;
    default:
        cs = 0;
        break;
    }

    // Same ranking as the original code: 16-bit UNORM > 10-bit > 8-bit.
    // R16G16B16A16_SFLOAT gets NO bonus.  The original never scored it, and
    // giving it one made P3 pick float16 over the BGRA8/10-bit UNORM formats
    // the renderer was written for (and HDR10 pick float16 over 10-bit).
    // It is still selectable where it is required: scRGB, via the color
    // space score above.
    int f = 0;
    if (is16u)     f = 2000;
    else if (is10) f = 1500;
    else if (is8)  f = 500;

    return cs + f;
}

bool format_usable_offscreen(VkPhysicalDevice gpu, VkFormat fmt)
{
    // Must match the usage requested in prepare_offscreen_buffers()
    // (COLOR_ATTACHMENT | TRANSFER_SRC) plus blending, which the viewport uses.
    VkFormatProperties p;
    vkGetPhysicalDeviceFormatProperties(gpu, fmt, &p);
    const VkFormatFeatureFlags need =
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
        VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
    return (p.optimalTilingFeatures & need) == need;
}

} // anonymous namespace


std::vector<VkSurfaceFormatKHR> Fl_Vk_Window_Driver::query_surface_formats()
{
    std::vector<VkSurfaceFormatKHR> out;
    VkResult result;

    // Only use the *2 entry point when the instance extension is enabled; a
    // non-null vkGetInstanceProcAddr result does not prove that.
    PFN_vkGetPhysicalDeviceSurfaceFormats2KHR fp2 = nullptr;
    for (const char* e : pWindow->ctx.instance_extensions)
        if (!strcmp(e, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME))
            fp2 = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormats2KHR>(
                vkGetInstanceProcAddr(pWindow->instance(),
                                      "vkGetPhysicalDeviceSurfaceFormats2KHR"));

    if (fp2) {
        VkPhysicalDeviceSurfaceInfo2KHR info = {};
        info.sType   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
        info.surface = pWindow->m_surface;

        uint32_t n = 0;
        result = fp2(pWindow->gpu(), &info, &n, nullptr);
        VK_CHECK(result);

        VkSurfaceFormat2KHR init = {};
        init.sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
        std::vector<VkSurfaceFormat2KHR> f2(n, init);
        result = fp2(pWindow->gpu(), &info, &n, f2.data());
        VK_CHECK(result);
        f2.resize(n);
        for (const auto& f : f2) out.push_back(f.surfaceFormat);
    } else {
        uint32_t n = 0;
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(
            pWindow->gpu(), pWindow->m_surface, &n, nullptr);
        VK_CHECK(result);
        out.resize(n);
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(
            pWindow->gpu(), pWindow->m_surface, &n, out.data());
        VK_CHECK(result);
        out.resize(n);
    }
    return out;
}

std::vector<VkSurfaceFormatKHR>
Fl_Vk_Window_Driver::headless_candidates(Fl_Vk_Output_Profile profile)
{
    std::vector<VkSurfaceFormatKHR> want;
    switch (profile) {
    case Fl_Vk_Output_Profile::HDR10:
        want.push_back({VK_FORMAT_A2B10G10R10_UNORM_PACK32,
                        VK_COLOR_SPACE_HDR10_ST2084_EXT});
        want.push_back({VK_FORMAT_R16G16B16A16_SFLOAT,
                        VK_COLOR_SPACE_HDR10_ST2084_EXT});
        break;
    case Fl_Vk_Output_Profile::ScRGB:
        want.push_back({VK_FORMAT_R16G16B16A16_SFLOAT,
                        VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT});
        break;
    case Fl_Vk_Output_Profile::WideGamut:
        want.push_back({VK_FORMAT_A2B10G10R10_UNORM_PACK32,
                        VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT});
        want.push_back({VK_FORMAT_R8G8B8A8_UNORM,
                        VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT});
        break;
    case Fl_Vk_Output_Profile::SDR:
        break;
    }
    // Always end with SDR so an unsupported profile still yields something.
    want.push_back({VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
    want.push_back({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});

    std::vector<VkSurfaceFormatKHR> out;
    for (const auto& w : want)
        if (format_usable_offscreen(pWindow->gpu(), w.format))
            out.push_back(w);
    return out;
}

void Fl_Vk_Window_Driver::select_surface_format()
{
    Fl_Vk_Display_Caps caps;
    std::vector<VkSurfaceFormatKHR> candidates;

    if (is_headless()) {
        // No surface, no display: the application chooses the target.
        const Fl_Vk_Output_Profile profile = pWindow->headless_output_profile();
        caps       = fl_vk_caps_for_profile(profile);
        candidates = headless_candidates(profile);
    } else {
        caps       = pWindow->display_caps();      // replaces has_hdr_monitor()
        candidates = query_surface_formats();
    }

    const int logLevel = pWindow->log_level();

    VkSurfaceFormatKHR sel = { VK_FORMAT_B8G8R8A8_UNORM,
                               VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };

    // Handle the "no preferred format" case first.
    const bool undefined = (candidates.size() == 1 &&
                            candidates[0].format == VK_FORMAT_UNDEFINED);
    if (!candidates.empty() && !undefined) {
        int best = -1;
        size_t bestIdx = 0;
        for (size_t i = 0; i < candidates.size(); ++i) {
            const int s = score_pair(candidates[i], caps);
            if (logLevel > 1) {
                printf("[%zu] format = %s color space = %s score = %d\n", i,
                       string_VkFormat(candidates[i].format),
                       string_VkColorSpaceKHR(candidates[i].colorSpace), s);
            }
            if (s > best) { best = s; bestIdx = i; }
        }
        if (best < 0) {
            if (logLevel > 1)
                fprintf(stderr, "No ideal format found, using fallback\n");
            bestIdx = 0;
        }
        sel = candidates[bestIdx];
    }

    pWindow->ctx.format      = sel.format;
    pWindow->ctx.colorSpace  = sel.colorSpace;
    pWindow->m_surfaceFormat = sel;

    if (logLevel > 2) {
        const char* what = is_headless() ? "offscreen" : "window";
        printf("\tSelected %s format = %s\n"
               "\tSelected %s color space = %s\n"
               "\tdisplay caps: hdr=%d wide_gamut=%d\n",
               what, string_VkFormat(sel.format),
               what, string_VkColorSpaceKHR(sel.colorSpace),
               caps.hdr, caps.wide_gamut);
    }
}
