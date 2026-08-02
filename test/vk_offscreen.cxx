//
// Headless/offscreen Vulkan demo program for the Fast Light Tool Kit (FLTK).
//
// This is vk_shape.cxx's twin: same shape, same shaders, same pipeline
// setup -- but rendered through Fl_Vk_Headless_Window instead of
// Fl_Vk_Window, with no native window ever created, no Fl::run() event
// loop, and no display connection required. Suitable for CLI/CI use
// (screenshot tests, batch rendering, servers without a display, etc).
//
// This library is free software. Distribution and use rights are outlined in
// the file "COPYING" which should have been included with this file.  If this
// file is missing or damaged, see the license at:
//
//     https://www.fltk.org/COPYING.php
//
// Please see the following page on how to report bugs and issues:
//
//     https://www.fltk.org/bugs.php
//

// Define 'HAVE_VK=1' on the compiler commandline to build this program
// w/o 'config.h' (needs FLTK lib with VK), for instance like:
//   $ fltk-config --use-vk --compile vk_offscreen.cxx -DHAVE_VK=1
// Use '-DHAVE_VK=0' to build and test w/o Vulkan support.

#ifndef HAVE_VK
#include <config.h> // needed only for 'HAVE_VK'
#endif

#if HAVE_VK

#include <iostream>

#define DBG std::cerr << __FUNCTION__ << " " << __LINE__ << std::endl;

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <FL/math.h>
#include <FL/Fl_Vk_Headless_Window.H>
#include <FL/Fl_Vk_Utils.H>
#include <FL/Fl_RGB_Image.H>
#include <Fl_Vk_Demos.H>   // Useless classes used for demo purposes only

// Needed only for capture_shape() below: capture_vk_rectangle() and the
// driver() accessor used to reach it live on Fl_Vk_Window_Driver, which is
// an internal driver-developer header, not part of Fl_Vk_Window's public
// API -- intentionally not wrapped by an Fl_RGB_Image-returning method on
// Fl_Vk_Window/Fl_Vk_Headless_Window itself. Adjust the include path to
// wherever your build keeps FLTK's src/ headers.
#include "../src/Fl_Vk_Window_Driver.H"

class vk_offscreen_window : public Fl_Vk_Headless_Window {
    bool vk_draw_begin() FL_OVERRIDE;
    void draw() FL_OVERRIDE;
public:
    int sides;
    vk_offscreen_window(int w, int h, const char *l = 0);
    ~vk_offscreen_window();

    // Vulkan overrides
    const char* application_name() FL_OVERRIDE { return "vk_offscreen"; }

    void prepare() FL_OVERRIDE;
    void destroy() FL_OVERRIDE;

    void destroy_mesh();
    void prepare_mesh();

    //! Renders one frame and reads it back into a newly-allocated
    //! Fl_RGB_Image. Caller owns the returned image. Returns NULL on
    //! failure (e.g. render_offscreen() couldn't initialize Vulkan).
    //!
    //! This is the method the task asked for -- it lives here, on the
    //! app-specific derived class, rather than on Fl_Vk_Window or
    //! Fl_Vk_Headless_Window themselves.
    Fl_RGB_Image *capture_shape();

protected:
    //! Shaders used in demo
    VkShaderModule m_vert_shader_module;
    VkShaderModule m_frag_shader_module;

    //! This is for holding a mesh
    Fl_Vk_Mesh m_mesh;

    //! Interface between shaders and desc.sets
    VkPipelineLayout m_pipeline_layout;

    void prepare_descriptor_layout();
    void prepare_render_pass();
    void prepare_pipeline();

private:
    VkShaderModule prepare_vs();
    VkShaderModule prepare_fs();
};

vk_offscreen_window::vk_offscreen_window(int w, int h, const char *l) :
Fl_Vk_Headless_Window(w, h, l) {
    mode(FL_RGB | FL_DOUBLE | FL_ALPHA);
    sides = 6;
    // Turn on validation, same as vk_shape.cxx
    m_validate = true;
    m_vert_shader_module = VK_NULL_HANDLE;
    m_frag_shader_module = VK_NULL_HANDLE;
}

vk_offscreen_window::~vk_offscreen_window()
{
    destroy();
}

// --- everything below through prepare_pipeline() is unchanged from
//     vk_shape_window: none of it touches the window system, so it works
//     identically whether the color attachment ends up in a swapchain
//     image or an offscreen one. ---

void vk_offscreen_window::prepare_mesh()
{
    // clang-format off
    struct Vertex
    {
        float x, y, z;  // 3D position
    };

    // Add the center vertex
    Vertex center = {0.0f, 0.0f, 0.0f};

    // Generate the outer vertices
    std::vector<Vertex> outerVertices(sides);
    for (int j = 0; j < sides; ++j) {
        double ang = j * 2 * M_PI / sides;
        float x = cos(ang);
        float y = sin(ang);
        outerVertices[j].x = x;
        outerVertices[j].y = y;
        outerVertices[j].z = 0.0f;
    }

    // Create the triangle list
    std::vector<Vertex> vertices;
    for (int i = 0; i < sides; ++i) {
        // First vertex of the triangle: the center
        vertices.push_back(center);

        // Second vertex: current outer vertex
        vertices.push_back(outerVertices[i]);

        // Third vertex: next outer vertex (wrap around for the last side)
        vertices.push_back(outerVertices[(i + 1) % sides]);
    }


    VkDeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();

    // clang-format on
    VkBufferCreateInfo buf_info = {};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.pNext = NULL;
    buf_info.size = buffer_size;
    buf_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buf_info.flags = 0;

    VkMemoryAllocateInfo mem_alloc = {};
    mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_alloc.pNext = NULL;
    mem_alloc.allocationSize = 0;
    mem_alloc.memoryTypeIndex = 0;

    VkResult result;
    void *data;

    memset(&m_mesh, 0, sizeof(m_mesh));

    result = vkCreateBuffer(device(), &buf_info, NULL, &m_mesh.buf);
    VK_CHECK(result);

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device(), m_mesh.buf, &mem_reqs);
    VK_CHECK(result);

    mem_alloc.allocationSize = mem_reqs.size;
    mem_alloc.memoryTypeIndex = findMemoryType(gpu(),
                                               mem_reqs.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    result = vkAllocateMemory(device(), &mem_alloc, NULL, &m_mesh.mem);
    VK_CHECK(result);

    result = vkMapMemory(device(), m_mesh.mem, 0,
                         mem_alloc.allocationSize, 0, &data);
    VK_CHECK(result);

    memcpy(data, vertices.data(), static_cast<size_t>(buffer_size));

    vkUnmapMemory(device(), m_mesh.mem);

    result = vkBindBufferMemory(device(), m_mesh.buf, m_mesh.mem, 0);
    VK_CHECK(result);

    m_mesh.vi_bindings[0].binding = 0;
    m_mesh.vi_bindings[0].stride = sizeof(vertices[0]);
    m_mesh.vi_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    m_mesh.vi_attrs[0].binding = 0;
    m_mesh.vi_attrs[0].location = 0;
    m_mesh.vi_attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    m_mesh.vi_attrs[0].offset = 0;
}

void vk_offscreen_window::prepare_render_pass()
{
    bool has_depth = mode() & FL_DEPTH;
    bool has_stencil = mode() & FL_STENCIL;

    VkAttachmentDescription attachments[2];
    attachments[0] = VkAttachmentDescription();
    attachments[0].format = format();
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Start undefined

    // *** The one line that actually differs from vk_shape.cxx ***
    // vk_shape.cxx uses VK_IMAGE_LAYOUT_PRESENT_SRC_KHR here, because its
    // color image is a swapchain image headed for a presentation engine.
    // This window is headless: there is no presentation engine, and
    // Fl_Vk_Headless_Window_Driver::capture_vk_rectangle() expects to find
    // the image in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL so it can
    // vkCmdCopyImageToBuffer() straight out of it.
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    attachments[1] = VkAttachmentDescription();

    VkAttachmentReference color_reference = {};
    color_reference.attachment = 0;
    color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_reference = {};
    depth_reference.attachment = 1;
    depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.flags = 0;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = NULL;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_reference;
    subpass.pResolveAttachments = NULL;

    if (has_depth || has_stencil)
    {
        attachments[1].format = m_depth.format;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        if (has_stencil)
        {
            attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        }
        else
        {
            attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments[1].finalLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        subpass.pDepthStencilAttachment = &depth_reference;
        subpass.preserveAttachmentCount = 0;
        subpass.pPreserveAttachments = NULL;
    }

    VkRenderPassCreateInfo rp_info = {};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.pNext = NULL;
    rp_info.attachmentCount = (has_depth || has_stencil) ? 2 : 1;
    rp_info.pAttachments = attachments;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    rp_info.dependencyCount = 0;
    rp_info.pDependencies = NULL;

    VkResult result;
    result = vkCreateRenderPass(device(), &rp_info, NULL, &m_renderPass);
    VK_CHECK(result);
}

VkShaderModule vk_offscreen_window::prepare_vs() {
    if (m_vert_shader_module != VK_NULL_HANDLE)
        return m_vert_shader_module;

    std::string vertex_shader_glsl = R"(
        #version 450
        layout(location = 0) in vec3 inPos;
        void main() {
            gl_Position = vec4(inPos, 1.0);
        }
    )";

    try {
        std::vector<uint32_t> spirv = compile_glsl_to_spirv(
            vertex_shader_glsl,
            shaderc_vertex_shader,
            "vertex_shader.glsl"
        );

        m_vert_shader_module = create_shader_module(device(), spirv);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        m_vert_shader_module = VK_NULL_HANDLE;
    }
    return m_vert_shader_module;
}

VkShaderModule vk_offscreen_window::prepare_fs() {
    if (m_frag_shader_module != VK_NULL_HANDLE)
        return m_frag_shader_module;

    std::string frag_shader_glsl = R"(
        #version 450

        layout(location = 0) out vec4 outColor;

        void main() {
            outColor = vec4(0.5, 0.6, 0.7, 1.0);
        }
    )";
    try {
        std::vector<uint32_t> spirv = compile_glsl_to_spirv(
            frag_shader_glsl,
            shaderc_fragment_shader,
            "frag_shader.glsl"
        );
        m_frag_shader_module = create_shader_module(device(), spirv);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        m_frag_shader_module = VK_NULL_HANDLE;
    }
    return m_frag_shader_module;
}

void vk_offscreen_window::prepare_pipeline() {
    VkGraphicsPipelineCreateInfo pipeline;
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo;

    VkPipelineVertexInputStateCreateInfo vi = {};
    VkPipelineInputAssemblyStateCreateInfo ia = {};
    VkPipelineRasterizationStateCreateInfo rs = {};
    VkPipelineColorBlendStateCreateInfo cb = {};
    VkPipelineDepthStencilStateCreateInfo ds = {};
    VkPipelineViewportStateCreateInfo vp = {};
    VkPipelineMultisampleStateCreateInfo ms = {};
    VkDynamicState dynamicStateEnables[(VK_DYNAMIC_STATE_STENCIL_REFERENCE - VK_DYNAMIC_STATE_VIEWPORT + 1)];
    VkPipelineDynamicStateCreateInfo dynamicState = {};

    VkResult result;

    memset(dynamicStateEnables, 0, sizeof dynamicStateEnables);
    memset(&dynamicState, 0, sizeof dynamicState);
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pDynamicStates = dynamicStateEnables;

    memset(&pipeline, 0, sizeof(pipeline));
    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.layout = m_pipeline_layout;

    memset(&vi, 0, sizeof(vi));
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.pNext = NULL;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = m_mesh.vi_bindings;
    vi.vertexAttributeDescriptionCount = 1;
    vi.pVertexAttributeDescriptions = m_mesh.vi_attrs;

    memset(&ia, 0, sizeof(ia));
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    memset(&rs, 0, sizeof(rs));
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rs.depthClampEnable = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.depthBiasEnable = VK_FALSE;
    rs.lineWidth = 1.0f;

    memset(&cb, 0, sizeof(cb));
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    VkPipelineColorBlendAttachmentState att_state[1];
    memset(att_state, 0, sizeof(att_state));
    att_state[0].colorWriteMask = 0xf;
    att_state[0].blendEnable = VK_FALSE;
    cb.attachmentCount = 1;
    cb.pAttachments = att_state;

    memset(&vp, 0, sizeof(vp));
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    dynamicStateEnables[dynamicState.dynamicStateCount++] =
        VK_DYNAMIC_STATE_VIEWPORT;
    vp.scissorCount = 1;
    dynamicStateEnables[dynamicState.dynamicStateCount++] =
        VK_DYNAMIC_STATE_SCISSOR;

    bool has_depth = mode() & FL_DEPTH;
    bool has_stencil = mode() & FL_STENCIL;

    memset(&ds, 0, sizeof(ds));
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = has_depth ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = has_depth ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.stencilTestEnable = has_stencil ? VK_TRUE : VK_FALSE;
    ds.back.failOp = VK_STENCIL_OP_KEEP;
    ds.back.passOp = VK_STENCIL_OP_KEEP;
    ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
    ds.front = ds.back;

    memset(&ms, 0, sizeof(ms));
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.pSampleMask = NULL;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    pipeline.stageCount = 2;
    VkPipelineShaderStageCreateInfo shaderStages[2];
    memset(&shaderStages, 0, 2 * sizeof(VkPipelineShaderStageCreateInfo));

    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = prepare_vs();
    shaderStages[0].pName = "main";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = prepare_fs();
    shaderStages[1].pName = "main";

    pipeline.pVertexInputState = &vi;
    pipeline.pInputAssemblyState = &ia;
    pipeline.pRasterizationState = &rs;
    pipeline.pColorBlendState = &cb;
    pipeline.pMultisampleState = &ms;
    pipeline.pViewportState = &vp;
    pipeline.pDepthStencilState = &ds;
    pipeline.pStages = shaderStages;
    pipeline.renderPass = m_renderPass;
    pipeline.pDynamicState = &dynamicState;

    memset(&pipelineCacheCreateInfo, 0, sizeof(pipelineCacheCreateInfo));
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    result = vkCreatePipelineCache(device(), &pipelineCacheCreateInfo, NULL,
                                   &pipelineCache());
    VK_CHECK(result);
    result = vkCreateGraphicsPipelines(device(), pipelineCache(), 1,
                                       &pipeline, NULL, &m_pipeline);
    VK_CHECK(result);

    vkDestroyPipelineCache(device(), pipelineCache(), NULL);
    pipelineCache() = VK_NULL_HANDLE;
}

void vk_offscreen_window::prepare_descriptor_layout() {
    VkResult result;

    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
    pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pPipelineLayoutCreateInfo.pNext = NULL;
    pPipelineLayoutCreateInfo.setLayoutCount = 0;
    pPipelineLayoutCreateInfo.pSetLayouts = NULL;

    result = vkCreatePipelineLayout(device(), &pPipelineLayoutCreateInfo, NULL,
                                    &m_pipeline_layout);
    VK_CHECK(result);
}

void vk_offscreen_window::prepare()
{
    prepare_mesh();
    prepare_descriptor_layout();
    prepare_render_pass();
    prepare_pipeline();
}

bool vk_offscreen_window::vk_draw_begin() {
    m_clearColor = { {1.0, 0, 0, 0} };  // Red background, same as vk_shape.cxx
    return Fl_Vk_Window::vk_draw_begin();
}

void vk_offscreen_window::draw() {
    // vk_shape.cxx also checks !shown() here; a headless window is never
    // shown(), so that check is dropped -- pixel_w()/pixel_h() being
    // positive (i.e. prepare_offscreen_buffers() having actually run) is
    // the right readiness signal here instead.
    if (pixel_w() <= 0 || pixel_h() <= 0)
        return;

    VkCommandBuffer cmd = getCurrentCommandBuffer();

    begin_render_pass(cmd);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    VkViewport viewport = {};
    viewport.width = static_cast<float>(pixel_w());
    viewport.height = static_cast<float>(pixel_h());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.extent.width = pixel_w();
    scissor.extent.height = pixel_h();
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_mesh.buf, offsets);
    vkCmdDraw(cmd, 3 * sides, 1, 0, 0); // Draw shape

    end_render_pass(cmd);
}

Fl_RGB_Image *vk_offscreen_window::capture_shape() {
    render_offscreen();  // renders exactly one frame: vk_draw_begin()/draw()/vk_draw_end()/swap_buffers()

    Fl_Vk_Window_Driver *drv = Fl_Vk_Window_Driver::driver(this);
    if (!drv) return NULL;

    return drv->capture_vk_rectangle(0, 0, pixel_w(), pixel_h());
}

void vk_offscreen_window::destroy_mesh()
{
    m_mesh.destroy(device());
}

void vk_offscreen_window::destroy()
{
    DBG;
    if (device() == VK_NULL_HANDLE)
        return;

    destroy_mesh();

    if (m_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device(), m_pipeline_layout, nullptr);
        m_pipeline_layout = VK_NULL_HANDLE;
    }
    if (m_vert_shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device(), m_vert_shader_module, nullptr);
        m_vert_shader_module = VK_NULL_HANDLE;
    }
    if (m_frag_shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device(), m_frag_shader_module, nullptr);
        m_frag_shader_module = VK_NULL_HANDLE;
    }
}

// Minimal, dependency-free PPM writer, purely so this test leaves behind
// something you can actually look at. FLTK itself doesn't ship a general
// image *writer* -- if you'd rather have PNG, encode img->data()[0]
// yourself with libpng/stb_image_write/etc.
static bool write_ppm(const char *path, Fl_RGB_Image *img) {
    if (!img || img->d() < 3) return false;
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    f << "P6\n" << img->w() << " " << img->h() << "\n255\n";

    const int d = img->d();
    const int ld = img->ld() ? img->ld() : img->w() * d;
    const uchar *data = (const uchar *)img->data()[0];

    for (int y = 0; y < img->h(); ++y) {
        const uchar *row = data + (size_t)y * ld;
        for (int x = 0; x < img->w(); ++x) {
            f.write((const char *)(row + (size_t)x * d), 3); // R,G,B
        }
    }
    return true;
}

int main(int argc, char **argv) {
    int sides = (argc > 1) ? atoi(argv[1]) : 6;
    if (sides < 3) sides = 3;

    vk_offscreen_window win(280, 280);
    win.sides = sides;

    Fl_RGB_Image* img = win.capture_shape();

    if (!img) {
        fprintf(stderr, "vk_offscreen: capture_shape() failed "
                        "(Vulkan/offscreen initialization error)\n");
        return 1;
    }

    printf("vk_offscreen: captured %dx%d image, %d channel(s)\n",
          img->w(), img->h(), img->d());

    const char *out_path = "vk_offscreen_out.ppm";
    if (write_ppm(out_path, img)) {
        printf("vk_offscreen: wrote %s (view with e.g. `convert %s out.png`)\n",
              out_path, out_path);
    } else {
        fprintf(stderr, "vk_offscreen: failed to write %s\n", out_path);
    }

    delete img; // alloc_array was set in capture_vk_rectangle(), so this
                // also frees the pixel buffer

    // win (and its Vulkan resources) tear down normally when it goes out
    // of scope here -- no Fl::run(), no shown() window, no display
    // connection was ever needed.
    return 0;
}

#else

#include <cstdio>
int main() {
    printf("vk_offscreen: this demo does not work without Vulkan (HAVE_VK=0)\n");
    return 1;
}

#endif
