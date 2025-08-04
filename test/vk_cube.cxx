//
// Vulkan test program for the Fast Light Tool Kit (FLTK).
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

#ifndef HAVE_VK
#include <config.h> // needed only for 'HAVE_VK'
#endif

// Uncomment the next line to test w/o Vulkan (see also above)
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

const double fps      = 60.0;     // desired frame rate (independent of speed slider)
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
#include <Fl_Vk_Demos.H>   // Useless classes used for demo purposes only
#include <FL/math.h>

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL 1
#include <glm/gtx/transform.hpp>

struct MVP
{
    glm::mat4x4 model;
    glm::mat4x4 view;
    glm::mat4x4 proj;
};

class cube_box : public Fl_Vk_Window {
    void vk_draw_begin() FL_OVERRIDE;
    void draw() FL_OVERRIDE;
    int handle(int) FL_OVERRIDE;

    void prepare_vertices();
    void prepare_uniform_buffer();
    void prepare_descriptor_layout();
    void prepare_descriptor_pool();
    void prepare_descriptor_set();
    void prepare_render_pass();
    void prepare_pipeline();
    VkShaderModule prepare_vs();
    VkShaderModule prepare_fs();
    
    //! Shaders
    VkShaderModule m_vert_shader;
    VkShaderModule m_frag_shader;
    
    //! This is for holding a mesh
    Fl_Vk_Mesh m_cube;
    
    //! Interface between shaders and desc.sets
    VkPipelineLayout      m_pipeline_layout;

    //! Additional Pipeline for wireframe mode
    VkPipeline m_wire_pipeline;
    
    //! Memory for descriptor sets
    VkDescriptorPool      m_desc_pool; 

    // Describe texture bindings whithin desc. set  
    VkDescriptorSetLayout m_desc_layout;

    // Actual data bound to shaders like texture or
    // uniform buffers
    VkDescriptorSet       m_desc_set; 
public:
    double lasttime;
    int wire;
    double size;
    double speed;

    void prepare() FL_OVERRIDE;
    void destroy() FL_OVERRIDE;
    const char* application_name() FL_OVERRIDE { return "vk_cube"; };
    
    cube_box(int x,int y,int w,int h,const char *l=0) : Fl_Vk_Window(x,y,w,h,l) {
        end();
        mode(FL_RGB | FL_DOUBLE | FL_ALPHA | FL_DEPTH);
        lasttime = 0.0;
        m_vert_shader = VK_NULL_HANDLE;
        m_frag_shader = VK_NULL_HANDLE;
        m_wire_pipeline = VK_NULL_HANDLE; 
        // Turn on validations
        m_validate = true;
    
    }
};



// m_format, m_depth (optionally) -> creates m_renderPass
void cube_box::prepare_render_pass() 
{
    bool has_depth = mode() & FL_DEPTH;
    bool has_stencil = mode() & FL_STENCIL;

    VkAttachmentDescription attachments[2] = {};
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
    else
    {
        subpass.pDepthStencilAttachment = NULL; // Explicitly set to NULL when no depth/stencil
    }
    
    // Add subpass self-dependency
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = 0; // Current subpass
    dependency.dstSubpass = 0; // Same subpass (self-dependency)
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo rp_info = {};
    rp_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.pNext = NULL;
    rp_info.attachmentCount = (has_depth || has_stencil) ? 2 : 1;
    rp_info.pAttachments = attachments;
    rp_info.subpassCount = 1;
    rp_info.pSubpasses = &subpass;
    rp_info.dependencyCount = 1; // Now we have one dependency
    rp_info.pDependencies = &dependency;
                    
    VkResult result;
    result = vkCreateRenderPass(device(), &rp_info, NULL, &m_renderPass);
    VK_CHECK(result);
}

VkShaderModule cube_box::prepare_vs() {
    // Example GLSL vertex shader
    std::string vertex_shader_glsl = R"(
        #version 450
        layout(location = 0) in vec3 inPos;
        layout(location = 1) in vec3 inColor;

        layout(location = 0) out vec3 fragColor;

        layout(set = 0, binding = 0) uniform MVP {
          mat4 model;
          mat4 view;
          mat4 proj;
        } mvp;

        void main() {
            gl_Position = mvp.proj * mvp.view * mvp.model * vec4(inPos, 1.0);
            fragColor = inColor;
        }
    )";
    
    try {
        std::vector<uint32_t> spirv = compile_glsl_to_spirv(
            vertex_shader_glsl,
            shaderc_vertex_shader,  // Shader type
            "vertex_shader.glsl"    // Filename for error reporting
        );

        m_vert_shader = create_shader_module(device(), spirv);
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        m_vert_shader = VK_NULL_HANDLE;
    }
    return m_vert_shader;
}

VkShaderModule cube_box::prepare_fs() {
    // Example GLSL vertex shader
    std::string frag_shader_glsl = R"(
        #version 450

        layout(location = 0) in vec3 fragColor;
        // Output color
        layout(location = 0) out vec4 outColor;

        void main() {
            outColor = vec4(fragColor, 1.0);
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
        m_frag_shader = create_shader_module(device(), spirv);
    
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return m_frag_shader;
}

// clang-format off
struct Vertex
{
    float pos[3];  // 3D position
    uint8_t color[3];  // color
};
    
std::vector<Vertex> vertices = {
    // Front face (blue) - ok
    {{0.0f, 0.0f, 0.0f}, {0, 0, 255}},
    {{1.0f, 0.0f, 0.0f}, {0, 0, 255}},
    {{1.0f, 1.0f, 0.0f}, {0, 0, 255}},
    {{0.0f, 1.0f, 0.0f}, {0, 0, 255}},

    // Back face (cyan) - ok
    {{0.0f, 0.0f, 1.0f}, {0, 255, 255}},
    {{1.0f, 0.0f, 1.0f}, {0, 255, 255}},
    {{1.0f, 1.0f, 1.0f}, {0, 255, 255}},
    {{0.0f, 1.0f, 1.0f}, {0, 255, 255}},

    // Left face (magenta)
    {{0.0f, 0.0f, 0.0f}, {255, 0, 255}},
    {{0.0f, 0.0f, 1.0f}, {255, 0, 255}},
    {{0.0f, 1.0f, 1.0f}, {255, 0, 255}},
    {{0.0f, 1.0f, 0.0f}, {255, 0, 255}},

    // Right face (red)
    {{1.0f, 0.0f, 0.0f}, {255, 0, 0}},
    {{1.0f, 0.0f, 1.0f}, {255, 0, 0}},
    {{1.0f, 1.0f, 1.0f}, {255, 0, 0}},
    {{1.0f, 1.0f, 0.0f}, {255, 0, 0}},

    // Top face (yellow)
    {{0.0f, 1.0f, 0.0f}, {255, 255, 0}},
    {{0.0f, 1.0f, 1.0f}, {255, 255, 0}},
    {{1.0f, 1.0f, 1.0f}, {255, 255, 0}},
    {{1.0f, 1.0f, 0.0f}, {255, 255, 0}},

    // Bottom face (green)
    {{0.0f, 0.0f, 0.0f}, {0, 255, 0}},
    {{0.0f, 0.0f, 1.0f}, {0, 255, 0}},
    {{1.0f, 0.0f, 1.0f}, {0, 255, 0}},
    {{1.0f, 0.0f, 0.0f}, {0, 255, 0}},
};

std::vector<uint16_t> indices = {
    0, 1, 2,  2, 3, 0,        // front
    4, 5, 6,  6, 7, 4,        // back
    8, 9,10, 10,11, 8,        // left
    12,13,14, 14,15,12,       // right
    16,17,18, 18,19,16,       // top
    20,21,22, 22,23,20        // bottom
};


void cube_box::prepare_pipeline() {
    VkGraphicsPipelineCreateInfo pipeline;
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo;

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

    memset(&vi, 0, sizeof(vi));
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.pNext = NULL;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = m_cube.vi_bindings;
    vi.vertexAttributeDescriptionCount = 2;
    vi.pVertexAttributeDescriptions = m_cube.vi_attrs;

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

    // Create fill pipeline
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    pipeline.pRasterizationState = &rs;
    result = vkCreateGraphicsPipelines(device(), pipelineCache(), 1, &pipeline, NULL, &m_pipeline);
    VK_CHECK(result);

    // Create wireframe pipeline
    rs.polygonMode = VK_POLYGON_MODE_LINE;
    result = vkCreateGraphicsPipelines(device(), pipelineCache(), 1, &pipeline, NULL, &m_wire_pipeline);
    VK_CHECK(result);
    
    vkDestroyPipelineCache(device(), pipelineCache(), NULL);
    pipelineCache() = VK_NULL_HANDLE;

}


void cube_box::prepare_vertices()
{
    
	VkDeviceSize buffer_size = sizeof(vertices[0]) * sizeof(vertices);
    
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

    memset(&m_cube, 0, sizeof(m_cube));

    VkDeviceSize index_buffer_size = sizeof(indices[0]) * indices.size();

    VkBufferCreateInfo index_buf_info = {};
    index_buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    index_buf_info.size = index_buffer_size;
    index_buf_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    index_buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    result = vkCreateBuffer(device(), &index_buf_info, nullptr, &m_cube.indexBuffer);
    VK_CHECK(result);

    // Get memory requirements
    VkMemoryRequirements index_mem_reqs;
    vkGetBufferMemoryRequirements(device(), m_cube.indexBuffer, &index_mem_reqs);

    // Allocate memory
    VkMemoryAllocateInfo index_alloc_info = {};
    index_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    index_alloc_info.allocationSize = index_mem_reqs.size;
    index_alloc_info.memoryTypeIndex = findMemoryType(gpu(),
                                                      index_mem_reqs.memoryTypeBits,
                                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    result = vkAllocateMemory(device(), &index_alloc_info, nullptr, &m_cube.indexMem);
    VK_CHECK(result);

    // Upload data
    void* index_data = nullptr;
    result = vkMapMemory(device(), m_cube.indexMem, 0, index_alloc_info.allocationSize, 0, &index_data);
    VK_CHECK(result);

    memcpy(index_data, indices.data(), static_cast<size_t>(index_buffer_size));
    vkUnmapMemory(device(), m_cube.indexMem);

    // Bind buffer to memory
    result = vkBindBufferMemory(device(), m_cube.indexBuffer, m_cube.indexMem, 0);
    VK_CHECK(result);

    result = vkCreateBuffer(device(), &buf_info, NULL, &m_cube.buf);
    VK_CHECK(result);

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device(), m_cube.buf, &mem_reqs);
    VK_CHECK(result);

    mem_alloc.allocationSize = mem_reqs.size;
    mem_alloc.memoryTypeIndex = findMemoryType(gpu(),
                                               mem_reqs.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    result = vkAllocateMemory(device(), &mem_alloc, NULL, &m_cube.mem);
    VK_CHECK(result);

    result = vkMapMemory(device(), m_cube.mem, 0,
                         mem_alloc.allocationSize, 0, &data);
    VK_CHECK(result);

	memcpy(data, vertices.data(), static_cast<size_t>(buffer_size));

    vkUnmapMemory(device(), m_cube.mem);

    result = vkBindBufferMemory(device(), m_cube.buf, m_cube.mem, 0);
    VK_CHECK(result);

    m_cube.vi_bindings[0].binding = 0;
    m_cube.vi_bindings[0].stride = sizeof(Vertex);
    m_cube.vi_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    m_cube.vi_attrs[0].location = 0;
    m_cube.vi_attrs[0].binding = 0;
    m_cube.vi_attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    m_cube.vi_attrs[0].offset = offsetof(Vertex, pos);
    
    m_cube.vi_attrs[1].location = 1;
    m_cube.vi_attrs[1].binding = 0;
    m_cube.vi_attrs[1].format = VK_FORMAT_R8G8B8_UNORM;
    m_cube.vi_attrs[1].offset = offsetof(Vertex, color);
}

void cube_box::prepare_uniform_buffer()
{
    VkBufferCreateInfo ubo_buf_info = {};
    ubo_buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ubo_buf_info.size = sizeof(MVP);
    ubo_buf_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    ubo_buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    vkCreateBuffer(device(), &ubo_buf_info, nullptr, &m_cube.uniformBuffer);
    
    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device(), m_cube.uniformBuffer, &mem_reqs);
    
    VkMemoryAllocateInfo ubo_alloc_info = {};
    ubo_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ubo_alloc_info.allocationSize = mem_reqs.size;
    ubo_alloc_info.memoryTypeIndex = findMemoryType(gpu(),
                                                    mem_reqs.memoryTypeBits,
                                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkAllocateMemory(device(), &ubo_alloc_info, nullptr, &m_cube.uniformMemory);
    vkBindBufferMemory(device(), m_cube.uniformBuffer, m_cube.uniformMemory, 0);
}

void cube_box::prepare_descriptor_pool()
{
    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    VkResult result = vkCreateDescriptorPool(device(), &poolInfo, nullptr, &m_desc_pool);
    VK_CHECK(result);
}

void cube_box::prepare_descriptor_set()
{
    VkResult result;

    // Create descriptor set layout
    VkDescriptorSetLayoutBinding uboLayoutBinding = {};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;

    result = vkCreateDescriptorSetLayout(device(), &layoutInfo, nullptr, &m_desc_layout);
    VK_CHECK(result);

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_desc_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &m_desc_layout;

    result = vkAllocateDescriptorSets(device(), &alloc_info, &m_desc_set);
    VK_CHECK(result);

    // Update descriptor set
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = m_cube.uniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(MVP);

    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = m_desc_set;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(device(), 1, &descriptorWrite, 0, nullptr);
}

void cube_box::prepare_descriptor_layout()
{
    VkResult result;

    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
    pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pPipelineLayoutCreateInfo.setLayoutCount = 1;
    pPipelineLayoutCreateInfo.pSetLayouts = &m_desc_layout;

    result = vkCreatePipelineLayout(device(), &pPipelineLayoutCreateInfo, nullptr, &m_pipeline_layout);
    VK_CHECK(result);
}

void cube_box::destroy()
{
    m_cube.destroy(device());

    if (m_wire_pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device(), m_wire_pipeline, nullptr);
        m_wire_pipeline = VK_NULL_HANDLE;
    }
    
    if (m_desc_layout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device(), m_desc_layout, nullptr);
        m_desc_layout = VK_NULL_HANDLE;
    }
    
    if (m_desc_pool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device(), m_desc_pool, nullptr);
        m_desc_pool = VK_NULL_HANDLE;
    }

    if (m_pipeline_layout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device(), m_pipeline_layout, nullptr);
        m_pipeline_layout = VK_NULL_HANDLE;
    }

    // m_desc_set is destroyed by the pool.

    if (m_frag_shader != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(device(), m_frag_shader, nullptr);
        m_frag_shader = VK_NULL_HANDLE;
    }
    if (m_vert_shader != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(device(), m_vert_shader, nullptr);
        m_vert_shader = VK_NULL_HANDLE;
    }
    
}

void cube_box::prepare()
{
    prepare_vertices();
    prepare_uniform_buffer();
    prepare_descriptor_pool();
    prepare_descriptor_set();
    prepare_descriptor_layout();
    prepare_render_pass();
    prepare_pipeline();
}


void cube_box::vk_draw_begin()
{
    // Background color
    m_clearColor = { 0.4f, 0.4f, 0.4f, 0.0 };
    Fl_Vk_Window::vk_draw_begin();
}

void cube_box::draw() {
    lasttime = lasttime + speed;

    MVP ubo{};
    ubo.model = glm::mat4(1.0);
    ubo.model *= glm::rotate((float)glm::radians(lasttime*1.6F),
                             glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.model *= glm::rotate((float)glm::radians(lasttime*4.2F),
                             glm::vec3(1.0f, 0.0f, 0.0f));
    ubo.model *= glm::rotate((float)glm::radians(lasttime*2.3F),
                             glm::vec3(0.0f, 1.0f, 0.0f));
    ubo.model *= glm::translate(glm::vec3(-1.F, 1.2F, -1.5F));
    ubo.model *= glm::scale(glm::vec3(size, size, size));
    ubo.view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -10.0f));
    ubo.proj  = glm::frustum(-1.0, 1.0, -1.0, 1.0, 2.0, 10000.0);
    // Create the projection matrix (equivalent to glFrustum)
    ubo.proj[1][1] *= -1; // Flip Y for Vulkan

    void* data;
    vkMapMemory(device(), m_cube.uniformMemory, 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(device(), m_cube.uniformMemory);
    
    VkCommandBuffer cmd = getCurrentCommandBuffer();
    if (!m_swapchain || !cmd || !isFrameActive()) {
        return;
    }
    
    // Set viewport
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)w();
    viewport.height = (float)h();
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    // Set scissor
    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = {(uint32_t)w(), (uint32_t)h()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    
    // Bind pipeline based on wire
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      wire ? m_wire_pipeline : m_pipeline);

    
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipeline_layout,
                            0, 1, &m_desc_set,
                            0, nullptr);

    // Draw the cube
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1,
                           &m_cube.buf, offsets);

    vkCmdBindIndexBuffer(cmd, m_cube.indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, 6*3*2, 1, 0, 0, 0); // 6 sides * 2 triangles * 3 verts

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
  grid->layout();

#if HAVE_VK
  //overlay_button(lt_cube);  // overlay a button onto the Vulkan window
#endif // HAVE_VK

  form->end();
  form->resizable(grid);
  form->size_range(form->w(), form->h()); // minimum window size
}

int main(int argc, char **argv) {
  Fl::set_color(FL_FREE_COLOR, 255, 255, 0, 75);
  makeform(argv[0]);

  speed->bounds(60, 0);
  speed->value(lt_cube->speed = 2.0);
  speed->callback(speed_cb);

  size->bounds(4, 0.2);
  size->value(lt_cube->size = 2.0);
  size->callback(size_cb);

  flat->value(1);
  flat->callback(flat_cb);
  wire->value(0);
  wire->callback(wire_cb);

  form->label("Vulkan Cube Demo");
  form->show(argc,argv);
  lt_cube->show();
  rt_cube->show();

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
