//
// OpenGL test program for the Fast Light Tool Kit (FLTK).
//
// Modified to have 2 cubes to test multiple OpenGL contexts
//
// Copyright 1998-2024 by Bill Spitzak and others.
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
// w/o 'config.h' (needs FLTK lib with GL), for instance like:
//   $ fltk-config --use-vk --compile cube.cxx -DHAVE_VK=1
// Use '-DHAVE_VK=0' to build and test w/o OpenGL support.

#ifndef HAVE_VK
#include <config.h> // needed only for 'HAVE_VL'
#endif

// ... or uncomment the next line to test w/o OpenGL (see also above)
// #undef HAVE_VK

#include <iostream>

#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Radio_Light_Button.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_Sys_Menu_Bar.H>
#include <FL/Fl_Printer.H>      // demo printing
#include <FL/Fl_Grid.H>         // grid layout

// Glocal constants and variables

const double fps      = 25.0;     // desired frame rate (independent of speed slider)
const double delay    = 1.0/fps;  // calculated timer delay
int count             = -2;       // initialize loop (draw) counter
int done              =  0;       // set to 1 in exit button callback
Fl_Timestamp start;               // taken at start of main or after reset

#define DEBUG_TIMER (0)           // >0: detailed debugging, see code (should be 0)

#if (DEBUG_TIMER)
const double dx       = 0.001;    // max. allowed deviation from delta time
const double d_min    = delay - dx;
const double d_max    = delay + dx;
#endif // DEBUG_TIMER

// Global pointers to widgets

class cube_box;

Fl_Window *form;
Fl_Slider *speed, *size;
Fl_Button *exit_button, *wire, *flat;
cube_box  *lt_cube, *rt_cube;

#if !HAVE_VK
class cube_box : public Fl_Box {
public:
  double lasttime;
  int wire;
  double size;
  double speed;
  cube_box(int x,int y,int w,int h,const char *l=0) : Fl_Box(FL_DOWN_BOX,x,y,w,h,l) {
    label("This demo does\nnot work without VK");
  }
};
#else
#include <FL/Fl_Vk_Utils.H>
#include <FL/Fl_Vk_Window.H>
#include <FL/vk.h>
#include <FL/math.h>

class cube_box : public Fl_Vk_Window {
    void vk_draw_begin() FL_OVERRIDE;
  void draw() FL_OVERRIDE;
  int handle(int) FL_OVERRIDE;

    void prepare() FL_OVERRIDE;
    void prepare_vertices();
    void prepare_descriptor_layout();
    void prepare_render_pass();
    void prepare_pipeline();
    void prepare_descriptor_pool();
    void prepare_descriptor_set();
    VkShaderModule prepare_vs();
    VkShaderModule prepare_fs();
    
    void destroy_resources() FL_OVERRIDE;
    
    //! Shaders
    VkShaderModule m_vert_shader_module;
    VkShaderModule m_frag_shader_module;
    
    //! This is for holding a mesh
    Fl_Vk_Mesh m_vertices;
public:
    double lasttime;
    int wire;
    double size;
    double speed;
    cube_box(int x,int y,int w,int h,const char *l=0) : Fl_Vk_Window(x,y,w,h,l) {
      end();
      mode(FL_RGB | FL_DOUBLE | FL_ALPHA);
    lasttime = 0.0;
    m_vert_shader_module = VK_NULL_HANDLE;
    m_frag_shader_module = VK_NULL_HANDLE;
    m_validate = true;
    
  }
    ~cube_box();
};

/* The cube definition */
float v0[3] = {0.0, 0.0, 0.0};
float v1[3] = {1.0, 0.0, 0.0};
float v2[3] = {1.0, 1.0, 0.0};
float v3[3] = {0.0, 1.0, 0.0};
float v4[3] = {0.0, 0.0, 1.0};
float v5[3] = {1.0, 0.0, 1.0};
float v6[3] = {1.0, 1.0, 1.0};
float v7[3] = {0.0, 1.0, 1.0};

#define v3f(x) glVertex3fv(x)

void drawcube(int wire) {
/* Draw a colored cube */
  // glBegin(wire ? GL_LINE_LOOP : GL_POLYGON);
  // glColor3ub(0,0,255);
  // v3f(v0); v3f(v1); v3f(v2); v3f(v3);
  // glEnd();
  // glBegin(wire ? GL_LINE_LOOP : GL_POLYGON);
  // glColor3ub(0,255,255); v3f(v4); v3f(v5); v3f(v6); v3f(v7);
  // glEnd();
  // glBegin(wire ? GL_LINE_LOOP : GL_POLYGON);
  // glColor3ub(255,0,255); v3f(v0); v3f(v1); v3f(v5); v3f(v4);
  // glEnd();
  // glBegin(wire ? GL_LINE_LOOP : GL_POLYGON);
  // glColor3ub(255,255,0); v3f(v2); v3f(v3); v3f(v7); v3f(v6);
  // glEnd();
  // glBegin(wire ? GL_LINE_LOOP : GL_POLYGON);
  // glColor3ub(0,255,0); v3f(v0); v3f(v4); v3f(v7); v3f(v3);
  // glEnd();
  // glBegin(wire ? GL_LINE_LOOP : GL_POLYGON);
  // glColor3ub(255,0,0); v3f(v1); v3f(v2); v3f(v6); v3f(v5);
  // glEnd();
}

cube_box::~cube_box()
{
    vkDestroyShaderModule(m_device, m_frag_shader_module, NULL);
    vkDestroyShaderModule(m_device, m_vert_shader_module, NULL);
}

// m_format, m_depth (optionally) -> creates m_renderPass
void cube_box::prepare_render_pass() 
{
    bool has_depth = mode() & FL_DEPTH;
    bool has_stencil = mode() & FL_STENCIL;

    VkAttachmentDescription attachments[2] = {};
    attachments[0] = VkAttachmentDescription();
    attachments[0].format = m_format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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
    else
    {
        subpass.pDepthStencilAttachment = NULL; // Explicitly set to NULL when no depth/stencil
    }

    VkRenderPassCreateInfo rp_info = {};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.pNext = NULL;
    rp_info.attachmentCount = (has_depth || has_stencil) ? 2: 1;
    rp_info.pAttachments = attachments;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    rp_info.dependencyCount = 0;
    rp_info.pDependencies = NULL;
                    
    VkResult result;
    result = vkCreateRenderPass(m_device, &rp_info, NULL, &m_renderPass);
    VK_CHECK_RESULT(result);
}

VkShaderModule cube_box::prepare_vs() {
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

        m_vert_shader_module = create_shader_module(m_device, spirv);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        m_vert_shader_module = VK_NULL_HANDLE;
    }
    return m_vert_shader_module;
}

VkShaderModule cube_box::prepare_fs() {
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
        m_frag_shader_module = create_shader_module(m_device, spirv);
    
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return m_frag_shader_module;
}

void cube_box::prepare_pipeline() {
    VkGraphicsPipelineCreateInfo pipeline;
    VkPipelineCacheCreateInfo pipelineCache;

    VkPipelineVertexInputStateCreateInfo vi;
    VkPipelineInputAssemblyStateCreateInfo ia;
    VkPipelineRasterizationStateCreateInfo rs;
    VkPipelineColorBlendStateCreateInfo cb;
    VkPipelineDepthStencilStateCreateInfo ds;
    VkPipelineViewportStateCreateInfo vp;
    VkPipelineMultisampleStateCreateInfo ms;
    VkDynamicState dynamicStateEnables[(VK_DYNAMIC_STATE_STENCIL_REFERENCE - VK_DYNAMIC_STATE_VIEWPORT + 1)];
    VkPipelineDynamicStateCreateInfo dynamicState;

    VkResult result;
    
    memset(dynamicStateEnables, 0, sizeof dynamicStateEnables);
    memset(&dynamicState, 0, sizeof dynamicState);
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pDynamicStates = dynamicStateEnables;

    memset(&pipeline, 0, sizeof(pipeline));
    pipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline.layout = m_pipeline_layout;

    vi = m_vertices.vi;

    memset(&ia, 0, sizeof(ia));
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;

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

    memset(&pipelineCache, 0, sizeof(pipelineCache));
    pipelineCache.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    result = vkCreatePipelineCache(m_device, &pipelineCache, NULL,
                                   &m_pipelineCache);
    VK_CHECK_RESULT(result);
    result = vkCreateGraphicsPipelines(m_device, m_pipelineCache, 1,
                                       &pipeline, NULL, &m_pipeline);
    VK_CHECK_RESULT(result);

    vkDestroyPipelineCache(m_device, m_pipelineCache, NULL);

    // vkDestroyShaderModule(m_device, m_frag_shader_module, NULL);
    // vkDestroyShaderModule(m_device, m_vert_shader_module, NULL);
}


void cube_box::prepare_descriptor_pool() {
    VkDescriptorPoolSize type_count = {};
    type_count.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    type_count.descriptorCount = 1;
    
    VkDescriptorPoolCreateInfo descriptor_pool = {};
    descriptor_pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool.pNext = NULL;
    descriptor_pool.maxSets = 1;
    descriptor_pool.poolSizeCount = 1;
    descriptor_pool.pPoolSizes = &type_count;

    VkResult result;
             
    result = vkCreateDescriptorPool(m_device, &descriptor_pool, NULL,
                                    &m_desc_pool);
    VK_CHECK_RESULT(result);
}

void cube_box::prepare_descriptor_set() {
    VkResult result;
    uint32_t i;

    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pNext = NULL;
    alloc_info.descriptorPool = m_desc_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &m_desc_layout;
        
    result = vkAllocateDescriptorSets(m_device, &alloc_info, &m_desc_set);
    VK_CHECK_RESULT(result);
}

void cube_box::prepare_descriptor_layout() {
    VkDescriptorSetLayoutBinding layout_binding = {};
    layout_binding.binding = 0;
    layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layout_binding.descriptorCount = 0;
    layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layout_binding.pImmutableSamplers = NULL;
  
    VkDescriptorSetLayoutCreateInfo descriptor_layout = {};
    descriptor_layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_layout.pNext = NULL;
    descriptor_layout.bindingCount = 1;
    descriptor_layout.pBindings = &layout_binding;
                 
    VkResult result;

    result = vkCreateDescriptorSetLayout(m_device, &descriptor_layout, NULL,
                                         &m_desc_layout);
    VK_CHECK_RESULT(result);

    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
    pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pPipelineLayoutCreateInfo.pNext = NULL;
    pPipelineLayoutCreateInfo.setLayoutCount = 1;
    pPipelineLayoutCreateInfo.pSetLayouts = &m_desc_layout;

    result = vkCreatePipelineLayout(m_device, &pPipelineLayoutCreateInfo, NULL,
                                    &m_pipeline_layout);
    VK_CHECK_RESULT(result);
}

void cube_box::prepare_vertices()
{
    
    // clang-format off
    struct Vertex
    {
        float x, y, z;  // 3D position
    };

    int sides = 3;
    std::vector<Vertex> vertices(sides);
    for (int j=0; j<sides; j++) {
        double ang = j*2*M_PI/sides;
        float x = cos(ang);
        float y = sin(ang);
        vertices[j].x = x;
        vertices[j].y = y;
        vertices[j].z = 0.F;
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

    memset(&m_vertices, 0, sizeof(m_vertices));

    result = vkCreateBuffer(m_device, &buf_info, NULL, &m_vertices.buf);
    VK_CHECK_RESULT(result);

    vkGetBufferMemoryRequirements(m_device, m_vertices.buf, &m_mem_reqs);
    VK_CHECK_RESULT(result);

    mem_alloc.allocationSize = m_mem_reqs.size;
    pass = memory_type_from_properties(m_mem_reqs.memoryTypeBits,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       &mem_alloc.memoryTypeIndex);

    result = vkAllocateMemory(m_device, &mem_alloc, NULL, &m_vertices.mem);
    VK_CHECK_RESULT(result);

    result = vkMapMemory(m_device, m_vertices.mem, 0,
                         mem_alloc.allocationSize, 0, &data);
    VK_CHECK_RESULT(result);

	memcpy(data, vertices.data(), static_cast<size_t>(buffer_size));

    vkUnmapMemory(m_device, m_vertices.mem);

    result = vkBindBufferMemory(m_device, m_vertices.buf, m_vertices.mem, 0);
    VK_CHECK_RESULT(result);

    m_vertices.vi.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    m_vertices.vi.pNext = NULL;
    m_vertices.vi.vertexBindingDescriptionCount = 1;
    m_vertices.vi.pVertexBindingDescriptions = m_vertices.vi_bindings;
    m_vertices.vi.vertexAttributeDescriptionCount = 2;
    m_vertices.vi.pVertexAttributeDescriptions = m_vertices.vi_attrs;

    m_vertices.vi_bindings[0].binding = 0;
    m_vertices.vi_bindings[0].stride = sizeof(vertices[0]);
    m_vertices.vi_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    m_vertices.vi_attrs[0].binding = 0;
    m_vertices.vi_attrs[0].location = 0;
    m_vertices.vi_attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    m_vertices.vi_attrs[0].offset = 0;

    m_vertices.vi_attrs[1].binding = 0;
    m_vertices.vi_attrs[1].location = 1;
    m_vertices.vi_attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    m_vertices.vi_attrs[1].offset = sizeof(float) * 3;
}

void cube_box::prepare()
{
    prepare_vertices();
    prepare_descriptor_layout();
    prepare_render_pass();
    prepare_pipeline();
    prepare_descriptor_pool();
    prepare_descriptor_set();
}


void cube_box::destroy_resources() {
    if (m_vertices.buf != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_vertices.buf, NULL);
        m_vertices.buf = VK_NULL_HANDLE;
    }

    if (m_vertices.mem != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_vertices.mem, NULL);
        m_vertices.mem = VK_NULL_HANDLE;
    }
}

void cube_box::vk_draw_begin()
{
    // Background color
    m_clearColor = { 0.0, 0.0, 0.0, 0.0 };

    Fl_Vk_Window::vk_draw_begin();
}

void cube_box::draw() {
    lasttime = lasttime + speed;

    // Draw the cube
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(m_draw_cmd, 0, 1,
                           &m_vertices.buf, offsets);

    vkCmdDraw(m_draw_cmd, 3, 1, 0, 0);
}

int cube_box::handle(int e) {
  switch (e) {
    case FL_ENTER: cursor(FL_CURSOR_CROSS);   break;
    case FL_LEAVE: cursor(FL_CURSOR_DEFAULT); break;
  }
  return Fl_Vk_Window::handle(e);
}

// callback for overlay button (Fl_Button on OpenGL scene)
void show_info_cb(Fl_Widget*, void*) {
  fl_message("This is an example of using FLTK widgets inside Vulkan windows.\n"
             "Multiple widgets can be added to Fl_Vk_Windows. They will be\n"
             "rendered as overlays over the scene.");
}

// overlay a button onto an OpenGL window (cube_box)
// but don't change the current group Fl_Group::current()
void overlay_button(cube_box *cube) {
  Fl_Group *curr = Fl_Group::current();
  Fl_Group::current(0);
  Fl_Widget *w = new Fl_Button(10, 10, 120, 30, "FLTK over GL");
  w->color(FL_FREE_COLOR);
  w->box(FL_BORDER_BOX);
  w->callback(show_info_cb);
  cube->add(w);
  Fl_Group::current(curr);
}

#endif // HAVE_VK

void exit_cb(Fl_Widget *w = NULL, void *v = NULL) {

#if HAVE_VK

  // display performance data on stdout and (for Windows!) in a message window

  double runtime = Fl::seconds_since(start);
  char buffer[120];

  sprintf(buffer, "Count =%5d, time = %7.3f sec, fps = %5.2f, requested: %5.2f",
          count, runtime, count / runtime, fps);
  printf("%s\n", buffer);
  fflush(stdout);

  int choice = fl_choice("%s", "E&xit", "&Continue", "&Reset", buffer);
  switch(choice) {
    case 0:                     // exit program, close all windows
      done = 1;
      Fl::hide_all_windows();
      break;
    case 2:                     // reset
      count = -2;
      printf("*** RESET ***\n");
      fflush(stdout);
      break;
    default:                    // continue
      break;
  }

#else
  done = 1;
  Fl::hide_all_windows();
#endif
}

#if HAVE_VK

void timer_cb(void *data) {
  static Fl_Timestamp last = Fl::now();
  static Fl_Timestamp now;
  count++;
  if (count == 0) {
    start = Fl::now();
    last  = start;
  } else if (count > 0) {
    now = Fl::now();
#if (DEBUG_TIMER)
    double delta = Fl::seconds_since(last);
    if ((delta < d_min || delta > d_max) && count > 1) {
      printf("Count =%5d, delay should be %7.5f but %7.5f is outside [%7.5f .. %7.5f]\n",
             count, delay, delta, d_min, d_max);
      fflush(stdout);
    }
#endif // DEBUG_TIMER
  }

#if (DEBUG_TIMER > 1)
  if (count > 0 && count <= DEBUG_TIMER) { // log the first N iterations
    double runtime = Fl::seconds_since(start);
    printf("Count =%5d, time = %7.3f sec, fps = %5.2f, requested: %5.2f\n",
            count, runtime, count / runtime, fps);
    fflush(stdout);
  }
#endif // (DEBUG_TIMER > 1)

  lt_cube->redraw();
  rt_cube->redraw();
  Fl::repeat_timeout(delay, timer_cb);
  last = Fl::now();
}

#endif // HAVE_VK

void speed_cb(Fl_Widget *w, void *) {
  lt_cube->speed = rt_cube->speed = ((Fl_Slider *)w)->value();
}

void size_cb(Fl_Widget *w, void *) {
  lt_cube->size = rt_cube->size = ((Fl_Slider *)w)->value();
}

void flat_cb(Fl_Widget *w, void *) {
  int flat = ((Fl_Light_Button *)w)->value();
  lt_cube->wire = 1 - flat;
  rt_cube->wire = flat;
}

void wire_cb(Fl_Widget *w, void *) {
  int wire = ((Fl_Light_Button *)w)->value();
  lt_cube->wire = wire;
  rt_cube->wire = 1 - wire;
}

// print screen demo
void print_cb(Fl_Widget *w, void *data) {
  Fl_Printer printer;
  Fl_Window *win = Fl::first_window();
  if (!win) return;
  if (printer.start_job(1)) return;
  if (printer.start_page()) return;
  printer.scale(0.5, 0.5);
  printer.print_widget(win);
  printer.end_page();
  printer.end_job();
}

// Create a form that allows resizing for A and C (VK windows) with B fixed size/centered:
//
//      |<--------------------------------------->|<---------------------->|
//      .          lt_cube            center      :       rt_cube          .
//      .            350                100       :         350            .
//      .  |<------------------->|  |<-------->|  |<------------------->|  .
//      ....................................................................
//      :  .......................  ............  .......................  :  __
//      :  :                     :  :          :  :                     :  :
//      :  :          A          :  :    B     :  :          C          :  :     h = 350
//      :  :                     :  :          :  :                     :  :
//      :  :.....................:  :..........:  :.....................:  :  __
//      :..................................................................:  __ MARGIN
//
//      |  |                     |  |          |  |
//     MARGIN                    GAP           GAP

#define MENUBAR_H 25    // menubar height
#define MARGIN    20    // fixed margin around widgets
#define GAP       20    // fixed gap between widgets

void makeform(const char *name) {
  // Widget's XYWH's
  int form_w = 800 + 2 * MARGIN + 2 * GAP;   // main window width
  int form_h = 350 + MENUBAR_H + 2 * MARGIN; // main window height

  // main window
  form = new Fl_Window(form_w, form_h, name);
  form->callback(exit_cb);
  // menu bar
  Fl_Sys_Menu_Bar *menubar = new Fl_Sys_Menu_Bar(0, 0, form_w, MENUBAR_H);
  menubar->add("File/Print window", FL_COMMAND+'p', print_cb);
  menubar->add("File/Quit",         FL_COMMAND+'q', exit_cb);

  // Fl_Grid (layout)
  Fl_Grid *grid = new Fl_Grid(0, MENUBAR_H, form_w, 350 + 2 * MARGIN);
  grid->layout(4, 4, MARGIN, GAP);
  grid->box(FL_FLAT_BOX);

  // set column and row weights to control resizing behavior
  int cwe[] = {50,  0,  0, 50}; // column weights
  int rwe[] = { 0,  0, 50,  0}; // row weights
  grid->col_weight(cwe, 4);     // set weights for resizing
  grid->row_weight(rwe, 4);     // set weights for resizing

  // set non-default gaps for special layout purposes and labels
  grid->row_gap(0,  0);         // no gap below wire button
  grid->row_gap(2, 50);         // gap below sliders for labels

  // left Vk window
  lt_cube = new cube_box(0, 0, 350, 350);

  // center group
  wire  = new Fl_Radio_Light_Button(    0, 0, 100, 25, "Wire");
  flat  = new Fl_Radio_Light_Button(    0, 0, 100, 25, "Flat");
  speed = new Fl_Slider(FL_VERT_SLIDER, 0, 0,  40, 90, "Speed");
  size  = new Fl_Slider(FL_VERT_SLIDER, 0, 0,  40, 90, "Size");
#if HAVE_VK
  exit_button = new Fl_Button(          0, 0, 100, 25, "Stats / E&xit");
#else
  exit_button = new Fl_Button(          0, 0, 100, 25, "E&xit");
#endif
  exit_button->callback(exit_cb);
  exit_button->tooltip("Display statistics (fps) and\nchoose to exit or continue\n");

  // right VK window
  rt_cube = new cube_box(0, 0, 350, 350);

  // assign widgets to grid positions (R=row, C=col) and sizes
  // RS=rowspan, CS=colspan: R, C, RS, CS, optional alignment
  grid->widget(lt_cube,      0, 0,  4,  1);
  grid->widget(wire,         0, 1,  1,  2);
  grid->widget(flat,         1, 1,  1,  2);
  grid->widget(speed,        2, 1,  1,  1, FL_GRID_VERTICAL);
  grid->widget(size,         2, 2,  1,  1, FL_GRID_VERTICAL);
  grid->widget(exit_button,  3, 1,  1,  2);
  grid->widget(rt_cube,      0, 3,  4,  1);

#if HAVE_VK
  //overlay_button(lt_cube);  // overlay a button onto the Vulkan window
#endif // HAVE_VK

  form->end();
  form->resizable(grid);
  form->size_range(form->w(), form->h()); // minimum window size
}

int main(int argc, char **argv) {
  Fl::use_high_res_VK(1);
  Fl::set_color(FL_FREE_COLOR, 255, 255, 0, 75);
  makeform(argv[0]);

  speed->bounds(6, 0);
  speed->value(lt_cube->speed = 2.0);
  speed->callback(speed_cb);

  size->bounds(4, 0.2);
  size->value(lt_cube->size = 2.0);
  size->callback(size_cb);

  flat->value(1);
  flat->callback(flat_cb);
  wire->value(0);
  wire->callback(wire_cb);

  form->label("Cube Demo");
  form->show(argc,argv);
  // lt_cube->show();
  // rt_cube->show();

  lt_cube->wire  = wire->value();
  lt_cube->size  = size->value();
  lt_cube->speed = speed->value();
  lt_cube->redraw();

  rt_cube->wire  = !wire->value();
  rt_cube->size  = lt_cube->size;
  rt_cube->speed = lt_cube->speed;
  rt_cube->redraw();

#if HAVE_VK

  // with Vulkan: use a timer for drawing and measure performance

  form->wait_for_expose();
  Fl::add_timeout(0.1, timer_cb);      // start timer
  int ret = Fl::run();
  return ret;

#else // w/o VK: no timer, no performance report

  return Fl::run();

#endif
}
