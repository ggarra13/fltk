//
// Tiny Vulkan demo program for the Fast Light Tool Kit (FLTK).
//
// Copyright 1998-2010 by Bill Spitzak and others.
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

#include <config.h>
#include <FL/platform.H>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Hor_Slider.H>
#include <FL/math.h>

#if HAVE_VK

#include <limits>
#include <FL/Fl_Vk_Window.H>
#include <FL/Fl_Vk_Utils.H>

class vk_shape_window : public Fl_Vk_Window {
    void vk_draw_begin() FL_OVERRIDE;
    void draw() FL_OVERRIDE;
public:
    int sides;
    vk_shape_window(int x,int y,int w,int h,const char *l=0);
    vk_shape_window(int w,int h,const char *l=0);
    ~vk_shape_window();

    void prepare() FL_OVERRIDE;
    void prepare_vertices();
    void destroy_resources() FL_OVERRIDE;
    
protected:
    //! Shaders used in GLFW demo
    VkShaderModule m_vert_shader_module;
    VkShaderModule m_frag_shader_module;
    
    //! This is for holding a mesh
    Fl_Vk_Mesh m_vertices;
    
    void prepare_descriptor_layout();
    void prepare_render_pass();
    void prepare_pipeline();
    void prepare_descriptor_pool();
    void prepare_descriptor_set();
    
private:
    VkShaderModule prepare_vs();
    VkShaderModule prepare_fs();

};

vk_shape_window::~vk_shape_window()
{
    vkDestroyShaderModule(m_device, m_frag_shader_module, NULL);
    vkDestroyShaderModule(m_device, m_vert_shader_module, NULL);
}

vk_shape_window::vk_shape_window(int x,int y,int w,int h,const char *l) :
Fl_Vk_Window(x,y,w,h,l) {
    sides = 3;
    m_vert_shader_module = VK_NULL_HANDLE;
    m_frag_shader_module = VK_NULL_HANDLE;
}

vk_shape_window::vk_shape_window(int w,int h,const char *l) :
Fl_Vk_Window(w,h,l) {
    sides = 3;
    m_vert_shader_module = VK_NULL_HANDLE;
    m_frag_shader_module = VK_NULL_HANDLE;
}

void vk_shape_window::prepare_vertices()
{
    // clang-format off
    struct Vertex
    {
        float x, y, z;  // 3D position
    };

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

// m_format, m_depth (optionally) -> creates m_renderPass
void vk_shape_window::prepare_render_pass() 
{
    bool has_depth = mode() & FL_DEPTH;
    bool has_stencil = mode() & FL_STENCIL;

    VkAttachmentDescription attachments[2];
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

        m_vert_shader_module = create_shader_module(m_device, spirv);
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
        m_frag_shader_module = create_shader_module(m_device, spirv);
    
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return m_frag_shader_module;
}

void vk_shape_window::prepare_pipeline() {
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


void vk_shape_window::prepare_descriptor_pool() {
    VkDescriptorPoolSize type_count = {};
    type_count.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    type_count.descriptorCount = 0; //DEMO_TEXTURE_COUNT;
    
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

void vk_shape_window::prepare_descriptor_set() {
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

void vk_shape_window::prepare()
{
    prepare_vertices();
    prepare_descriptor_layout();
    prepare_render_pass();
    prepare_pipeline();
    prepare_descriptor_pool();
    prepare_descriptor_set();
}


void vk_shape_window::vk_draw_begin()
{
    // Background color
    m_clearColor = { 0.0, 0.0, 0.0, 0.0 };

    Fl_Vk_Window::vk_draw_begin();
}

void vk_shape_window::draw() {
    if (!shown() || w() <= 0 || h() <= 0) return;

    // Draw the shape
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(m_draw_cmd, 0, 1,
                           &m_vertices.buf, offsets);

    vkCmdDraw(m_draw_cmd, sides, 1, 0, 0);
}

void vk_shape_window::destroy_resources() {
    if (m_vertices.buf != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_vertices.buf, NULL);
        m_vertices.buf = VK_NULL_HANDLE;
    }

    if (m_vertices.mem != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_vertices.mem, NULL);
        m_vertices.mem = VK_NULL_HANDLE;
    }
}


void vk_shape_window::prepare_descriptor_layout() {
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

#else

#include <FL/Fl_Box.H>
class vk_shape_window : public Fl_Box {
public:
  int sides;
  vk_shape_window(int x,int y,int w,int h,const char *l=0)
    :Fl_Box(FL_DOWN_BOX,x,y,w,h,l){
      label("This demo does\nnot work without Vulkan");
  }
};

#endif

// when you change the data, as in this callback, you must call redraw():
void sides_cb(Fl_Widget *o, void *p) {
  vk_shape_window *sw = (vk_shape_window *)p;
  sw->sides = int(((Fl_Slider *)o)->value());
  sw->prepare_vertices();
  sw->redraw();
}

int main(int argc, char **argv) {
    Fl::use_high_res_VK(1);

    Fl_Window window(300, 330);
  
// the shape window could be it's own window, but here we make it
// a child window:
        
    vk_shape_window sw(10, 10, 280, 280);

// // make it resize:
    //  window.size_range(300,330,0,0,1,1,1);
    // add a knob to control it:
    Fl_Hor_Slider slider(50, 295, window.w()-60, 30, "Sides:");
    slider.align(FL_ALIGN_LEFT);
    slider.step(1);
    slider.bounds(3,40);

    window.resizable(&sw);
    slider.value(sw.sides);
    slider.callback(sides_cb,&sw);
    window.end();
    window.show(argc,argv);
    
    return Fl::run();
}
