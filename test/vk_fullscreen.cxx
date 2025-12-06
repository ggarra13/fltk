//
//
// Copyright 1998-2025 by Bill Spitzak and others.
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
//   $ fltk-config --use-vk --compile cube.cxx -DHAVE_VK=1
// Use '-DHAVE_VK=0' to build and test w/o OpenVK support.

#ifndef HAVE_VK
#include <config.h> // needed only for 'HAVE_VK'
#endif

// ... or uncomment the next line to test w/o Vulkan (see also above)
// #undef HAVE_VK

#include <FL/Fl.H>
#include <FL/Fl_Single_Window.H>
#include <FL/Fl_Hor_Slider.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Menu_Button.H>
#include <FL/Fl_Toggle_Light_Button.H>
#include <FL/math.h>
#include <FL/fl_ask.H>
#include <FL/Fl_Browser.H>
#include <stdio.h>

#if HAVE_VK
#include <FL/Fl_Vk_Utils.H>
#include <FL/Fl_Vk_Window.H>
#include <FL/vk.h>
#include <Fl_Vk_Demos.H>   // Useless classes used for demo purposes only
#include <FL/math.h>

class vk_shape_window : public Fl_Vk_Window {
    bool vk_draw_begin() FL_OVERRIDE;
    void draw() FL_OVERRIDE;
public:
    int sides;
    vk_shape_window(int x,int y,int w,int h,const char *l=0);
    vk_shape_window(int w,int h,const char *l=0);

    // Vulkan overrides
    const char* application_name()  FL_OVERRIDE { return "vk_shape"; }

    void prepare() FL_OVERRIDE;
    void destroy() FL_OVERRIDE;


    void destroy_mesh();
    void prepare_mesh();
    
protected:
    //! Shaders used in demo
    VkShaderModule m_vert_shader_module;
    VkShaderModule m_frag_shader_module;
    
    //! This is for holding a mesh
    Fl_Vk_Mesh m_mesh;
    
    //! Interface between shaders and desc.sets
    VkPipelineLayout      m_pipeline_layout;
    
    void prepare_descriptor_layout();
    void prepare_render_pass();
    void prepare_pipeline();
    
private:
    void _init();
    VkShaderModule prepare_vs();
    VkShaderModule prepare_fs();
};
    
void vk_shape_window::_init()
{
    mode(FL_RGB | FL_DOUBLE | FL_ALPHA);
        sides = 3;
        // Turn on validations
        m_validate = true;
    m_vert_shader_module = VK_NULL_HANDLE;
    m_frag_shader_module = VK_NULL_HANDLE;
}
    
vk_shape_window::vk_shape_window(int x,int y,int w,int h,const char *l) :
Fl_Vk_Window(x,y,w,h,l) {
    _init();
    }

vk_shape_window::vk_shape_window(int w,int h,const char *l) :
Fl_Vk_Window(w,h,l) {
    _init();
}

void vk_shape_window::prepare_mesh()
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
    bool pass;
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

// m_depth (optionally) -> creates m_renderPass
void vk_shape_window::prepare_render_pass() 
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
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // Final layout for presentation

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

VkShaderModule vk_shape_window::prepare_vs() {
    if (m_vert_shader_module != VK_NULL_HANDLE)
        return m_vert_shader_module;
    
    // Example GLSL vertex shader
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
            shaderc_vertex_shader,  // Shader type
            "vertex_shader.glsl"    // Filename for error reporting
        );

        m_vert_shader_module = create_shader_module(device(), spirv);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        m_vert_shader_module = VK_NULL_HANDLE;
    }
    return m_vert_shader_module;
}

VkShaderModule vk_shape_window::prepare_fs() {
    if (m_frag_shader_module != VK_NULL_HANDLE)
        return m_frag_shader_module;
    
    // Example GLSL vertex shader
    std::string frag_shader_glsl = R"(
        #version 450

        // Output color
        layout(location = 0) out vec4 outColor;

        void main() {
            outColor = vec4(0.5, 0.6, 0.7, 1.0);
        }
    )";
    // Compile to SPIR-V
    try {

        std::vector<uint32_t> spirv = compile_glsl_to_spirv(
            frag_shader_glsl,
            shaderc_fragment_shader,  // Shader type
            "frag_shader.glsl"    // Filename for error reporting
        );
        // Assuming you have a VkDevice 'device' already created
        m_frag_shader_module = create_shader_module(device(), spirv);
    
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        m_frag_shader_module = VK_NULL_HANDLE;
    }
    return m_frag_shader_module;
}

void vk_shape_window::prepare_pipeline() {
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

    // Two stages: vs and fs
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
    
void vk_shape_window::prepare_descriptor_layout() {
    VkResult result;
    bool pass;
    void *data;

    // These shaders don't have UBO parameters
    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
    pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pPipelineLayoutCreateInfo.pNext = NULL;
    pPipelineLayoutCreateInfo.setLayoutCount = 0;
    pPipelineLayoutCreateInfo.pSetLayouts = NULL;

    result = vkCreatePipelineLayout(device(), &pPipelineLayoutCreateInfo, NULL,
                                    &m_pipeline_layout);
    VK_CHECK(result);
}

void vk_shape_window::destroy()
{
    m_mesh.destroy(device());

    if (m_pipeline_layout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device(), m_pipeline_layout, nullptr);
        m_pipeline_layout = VK_NULL_HANDLE;
    }

    if (m_frag_shader_module != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(device(), m_frag_shader_module, nullptr);
        m_frag_shader_module = VK_NULL_HANDLE;
    }
    
    if (m_vert_shader_module != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(device(), m_vert_shader_module, nullptr);
        m_vert_shader_module = VK_NULL_HANDLE;
    }
    
}

void vk_shape_window::prepare()
{
    prepare_mesh();
    prepare_descriptor_layout();
    prepare_render_pass();
    prepare_pipeline();
}

bool vk_shape_window::vk_draw_begin() {
    m_clearColor = { 1.0, 0, 0, 0 };  // Red background
    return Fl_Vk_Window::vk_draw_begin();
}

void vk_shape_window::draw() {
    if (!shown() || w() <= 0 || h() <= 0)
        return;
    
    VkCommandBuffer cmd = getCurrentCommandBuffer();
    if (!m_swapchain || !cmd || !isFrameActive()) {
        return;
    }

    begin_render_pass(cmd);
    
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    VkViewport viewport = {};
    viewport.width = static_cast<float>(w());
    viewport.height = static_cast<float>(h());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    // Set scissor
    VkRect2D scissor = {};
    scissor.extent.width = w();
    scissor.extent.height = h();
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_mesh.buf, offsets);
    vkCmdDraw(cmd, 3 * sides, 1, 0, 0); // Draw shape

    end_render_pass(cmd);
}

void vk_shape_window::destroy_mesh()
{
    m_mesh.destroy(device());
}

// callback for overlay button (Fl_Button on OpenGL scene)
void show_info_cb(Fl_Widget*, void*) {
  fl_message("This is an example of using FLTK widgets inside Vulkan windows.\n"
             "Multiple widgets can be added to Fl_Vk_Windows. They will be\n"
             "rendered as overlays over the scene.");
}

// overlay a button onto an OpenGL window (vk_shape_window)
// but don't change the current group Fl_Group::current()
void overlay_button(vk_shape_window *cube) {
  Fl_Group *curr = Fl_Group::current();
  Fl_Group::current(0);
  Fl_Widget *w = new Fl_Button(10, 10, 120, 30, "FLTK over GL");
  w->color(FL_FREE_COLOR);
  w->box(FL_BORDER_BOX);
  w->callback(show_info_cb);
  cube->add(w);
  Fl_Group::current(curr);
}

void exit_cb(Fl_Widget *w = NULL, void *v = NULL) {
    exit(0);
}


// print screen demo
void print_cb(Fl_Widget *w, void *data) {
  // Fl_Printer printer;
  // Fl_Window *win = Fl::first_window();
  // if (!win) return;
  // if (printer.start_job(1)) return;
  // if (printer.start_page()) return;
  // printer.scale(0.5, 0.5);
  // printer.print_widget(win);
  // printer.end_page();
  // printer.end_job();
}

#else

#include <FL/fl_draw.H>

class vk_shape_window : public Fl_Window {
  void draw() FL_OVERRIDE;
public:
  int sides;
  vk_shape_window(int x,int y,int w,int h,const char *l=0);
};

vk_shape_window::vk_shape_window(int x,int y,int w,int h,const char *l) :
Fl_Window(x,y,w,h,l) {
  sides = 3;
}

void vk_shape_window::draw() {
  fl_color(0);
  fl_rectf(0,0,w(),h());
  fl_font(0,20);
  fl_color(7);
  fl_draw("This requires Vulkan",0,0,w(),h(),FL_ALIGN_CENTER);
}

#endif

class fullscreen_window : public Fl_Single_Window {
  public:
  fullscreen_window(int W, int H, const char *t=0) : Fl_Single_Window(W, H, t) {}
  void resize(int x, int y, int w, int h) FL_OVERRIDE;
  Fl_Toggle_Light_Button *border_button;
  Fl_Toggle_Light_Button *maximize_button;
  Fl_Toggle_Light_Button *fullscreen_button;
  Fl_Toggle_Light_Button *allscreens_button;
};


void after_resize(fullscreen_window *win) {
  if (win->maximize_active()) win->maximize_button->set();
  else win->maximize_button->clear();
  win->maximize_button->redraw();
  if (win->fullscreen_active()) win->fullscreen_button->set();
  else win->fullscreen_button->clear();
  win->fullscreen_button->redraw();
}


void fullscreen_window::resize(int x, int y, int w, int h) {
  Fl_Single_Window::resize(x,y,w,h);
  Fl::add_timeout(0, (Fl_Timeout_Handler)after_resize, this);
}


void sides_cb(Fl_Widget *o, void *p) {
  vk_shape_window *sw = (vk_shape_window *)p;
  sw->sides = int(((Fl_Slider *)o)->value());
  sw->wait_queue();
  sw->destroy_mesh();
  sw->prepare_mesh();
  sw->redraw();
}

void double_cb(Fl_Widget *, void *) {}


void border_cb(Fl_Button *b, Fl_Window *w) {
  int d = b->value();
  w->border(d);
  // border change may have been refused (e.g. with fullscreen window)
  if ((int)w->border() != d) b->value(w->border());
}


void maximize_cb(Fl_Button *b, Fl_Window *w) {
  if (w->fullscreen_active()) {
    b->value(1 - b->value());
    return;
  }
  if (w->maximize_active()) {
    w->un_maximize();
  } else {
    w->maximize();
  }
}


void fullscreen_cb(Fl_Button *b, Fl_Window *w) {
  if (w->maximize_active()) {
    b->value(1 - b->value());
    return;
  }
  if (b->value()) {
    w->fullscreen();
  } else {
    w->fullscreen_off();
  }
}

void allscreens_cb(Fl_Widget *o, void *p) {
  Fl_Window *w = (Fl_Window *)p;
  int d = ((Fl_Button *)o)->value();
  if (d) {
    int top, bottom, left, right;
    int top_y, bottom_y, left_x, right_x;

    int sx, sy, sw, sh;

    top = bottom = left = right = 0;

    Fl::screen_xywh(sx, sy, sw, sh, 0);
    top_y = sy;
    bottom_y = sy + sh;
    left_x = sx;
    right_x = sx + sw;

    for (int i = 1;i < Fl::screen_count();i++) {
      Fl::screen_xywh(sx, sy, sw, sh, i);
      if (sy < top_y) {
        top = i;
        top_y = sy;
      }
      if ((sy + sh) > bottom_y) {
        bottom = i;
        bottom_y = sy + sh;
      }
      if (sx < left_x) {
        left = i;
        left_x = sx;
      }
      if ((sx + sw) > right_x) {
        right = i;
        right_x = sx + sw;
      }
    }

    w->fullscreen_screens(top, bottom, left, right);
  } else {
    w->fullscreen_screens(-1, -1, -1, -1);
  }
}

void update_screeninfo(Fl_Widget *b, void *p) {
    Fl_Browser *browser = (Fl_Browser *)p;
    int x, y, w, h;
    char line[128];
    browser->clear();

    snprintf(line, sizeof(line), "Main screen work area: %dx%d@%d,%d", Fl::w(), Fl::h(), Fl::x(), Fl::y());
    browser->add(line);
    Fl::screen_work_area(x, y, w, h);
    snprintf(line, sizeof(line), "Mouse screen work area: %dx%d@%d,%d", w, h, x, y);
    browser->add(line);
    for (int n = 0; n < Fl::screen_count(); n++) {
        int x, y, w, h;
        float dpih, dpiv;
        Fl::screen_xywh(x, y, w, h, n);
        Fl::screen_dpi(dpih, dpiv, n);
        snprintf(line, sizeof(line), "Screen %d: %dx%d@%d,%d DPI:%.1fx%.1f scale:%.2f", n, w, h, x, y, dpih, dpiv, Fl::screen_scale(n));
        browser->add(line);
        Fl::screen_work_area(x, y, w, h, n);
        snprintf(line, sizeof(line), "Work area %d: %dx%d@%d,%d", n, w, h, x, y);
        browser->add(line);
    }
}


#define NUMB 9

int twowindow = 0;
int initfull = 0;
int parse_args(int, char **argv, int &i) {
  if (argv[i][1] == '2') {twowindow = 1; i++; return 1;}
  if (argv[i][1] == 'f') {initfull = 1; i++; return 1;}
  return 0;
}

int main(int argc, char **argv) {

  Fl::use_high_res_VK(1);
  int i=0;
  if (Fl::args(argc,argv,i,parse_args) < argc)
    Fl::fatal("Options are:\n -2 = 2 windows\n -f = startup fullscreen\n%s",Fl::help);

  fullscreen_window window(460,400+30*NUMB); window.end();

  vk_shape_window sw(10,10,window.w()-20,window.h()-30*NUMB-120);
  sw.set_visible(); // necessary because sw is not a child of window
#if HAVE_GL
  sw.mode(FL_RGB);
#endif

  Fl_Window *w;
  if (twowindow) {      // make its own window
    sw.resizable(&sw);
    w = &sw;
    window.set_modal(); // makes controls stay on top when fullscreen pushed
    argc--;
    sw.show();
  } else {              // otherwise make a subwindow
    window.add(sw);
    window.resizable(&sw);
    w = &window;
  }

  window.begin();

  int y = window.h()-30*NUMB-105;
  Fl_Hor_Slider slider(50,y,window.w()-60,30,"Sides:");
  slider.align(FL_ALIGN_LEFT);
  slider.callback(sides_cb,&sw);
  slider.value(sw.sides);
  slider.step(1);
  slider.bounds(3,40);
  y+=30;

  Fl_Input i1(50,y,window.w()-60,30, "Input");
  y+=30;

  window.border_button = new Fl_Toggle_Light_Button(50,y,window.w()-60,30,"Border");
  window.border_button->callback((Fl_Callback*)border_cb,w);
  window.border_button->set();
  y+=30;

  window.fullscreen_button = new Fl_Toggle_Light_Button(50,y,window.w()-60,30,"FullScreen");
  window.fullscreen_button->callback((Fl_Callback*)fullscreen_cb,w);
  y+=30;

  window.maximize_button = new Fl_Toggle_Light_Button(50,y,window.w()-60,30,"Maximize");
  window.maximize_button->callback((Fl_Callback*)maximize_cb,w);
  y+=30;

  window.allscreens_button = new Fl_Toggle_Light_Button(50,y,window.w()-60,30,"All Screens");
  window.allscreens_button->callback(allscreens_cb,w);
  y+=30;

  Fl_Button eb(50,y,window.w()-60,30,"Exit");
  eb.callback(exit_cb);
  y+=30;

  Fl_Browser *browser = new Fl_Browser(50,y,window.w()-60,100);
  update_screeninfo(0, browser);
  y+=100;

  Fl_Button update(50,y,window.w()-60,30,"Update");
  update.callback(update_screeninfo, browser);
  y+=30;

  if (initfull) {window.fullscreen_button->set(); window.fullscreen_button->do_callback();}

  window.end();
  window.show(argc,argv);

  return Fl::run();
}
