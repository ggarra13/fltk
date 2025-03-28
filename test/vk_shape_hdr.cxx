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

#define NOMINMAX
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

#define DEMO_TEXTURE_COUNT 1

class DynamicTextureWindow : public Fl_Vk_Window {
    void vk_draw_begin() FL_OVERRIDE;
    void draw() FL_OVERRIDE;
public:
    int sides;
    DynamicTextureWindow(int x,int y,int w,int h,const char *l=0);
    DynamicTextureWindow(int w,int h,const char *l=0);
    ~DynamicTextureWindow();

    float depthIncrement = -0.01f;
    
    void prepare() FL_OVERRIDE;
    void destroy_resources() FL_OVERRIDE;
    void prepare_vertices();
    void update_texture();

    static void texture_cb(DynamicTextureWindow* w)
        {
            w->redraw();
            Fl::repeat_timeout(1.0/60.0, (Fl_Timeout_Handler)texture_cb, w);
        }
    
protected:
    //! Shaders used in GLFW demo
    VkShaderModule m_vert_shader_module;
    VkShaderModule m_frag_shader_module;
    uint32_t frame_counter = 0;
    
    //! This is for holding a mesh
    Fl_Vk_Mesh m_vertices;
    Fl_Vk_Texture  m_textures[DEMO_TEXTURE_COUNT];
    
    void prepare_textures();
    void prepare_descriptor_layout();
    void prepare_render_pass();
    void prepare_pipeline();
    void prepare_descriptor_pool();
    void prepare_descriptor_set();
    
private:
    void prepare_texture_image(const float *tex_colors,
                               Fl_Vk_Texture* tex_obj,
                               VkImageTiling tiling,
                               VkImageUsageFlags usage,
                               VkFlags required_props);

    VkShaderModule prepare_vs();
    VkShaderModule prepare_fs();
};

static void depth_stencil_cb(DynamicTextureWindow* w)
{
    if (w->m_depthStencil > 0.99f)
        w->depthIncrement = -0.001f;
    if (w->m_depthStencil < 0.8f)
        w->depthIncrement = 0.001f;

    w->m_depthStencil += w->depthIncrement;
    w->redraw();
    //Fl::repeat_timeout(0.001, (Fl_Timeout_Handler) depth_stencil_cb, w);
}

DynamicTextureWindow::~DynamicTextureWindow()
{
    vkDestroyShaderModule(m_device, m_frag_shader_module, NULL);
    vkDestroyShaderModule(m_device, m_vert_shader_module, NULL);
}

DynamicTextureWindow::DynamicTextureWindow(int x,int y,int w,int h,const char *l) :
Fl_Vk_Window(x,y,w,h,l) {
    mode(FL_RGB | FL_DOUBLE | FL_ALPHA | FL_DEPTH);
    sides = 3;
    m_validate = true;
    m_vert_shader_module = VK_NULL_HANDLE;
    m_frag_shader_module = VK_NULL_HANDLE;
}

DynamicTextureWindow::DynamicTextureWindow(int w,int h,const char *l) :
Fl_Vk_Window(w,h,l) {
    mode(FL_RGB | FL_DOUBLE | FL_ALPHA | FL_DEPTH);
    sides = 3;
    m_validate = true;
    m_vert_shader_module = VK_NULL_HANDLE;
    m_frag_shader_module = VK_NULL_HANDLE;
}

void DynamicTextureWindow::prepare_texture_image(const float *tex_colors,
                                                 Fl_Vk_Texture* tex_obj,
                                                 VkImageTiling tiling,
                                                 VkImageUsageFlags usage,
                                                 VkFlags required_props) {
    const VkFormat tex_format =  VK_FORMAT_R32G32B32A32_SFLOAT;
        // was VK_FORMAT_B8G8R8A8_UNORM;
    const int32_t tex_width = 2;
    const int32_t tex_height = 2;
    VkResult result;
    bool pass;

    tex_obj->width = tex_width;
    tex_obj->height = tex_height;

    VkImageCreateInfo image_create_info = {};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.pNext = NULL;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = tex_format;
    image_create_info.extent = {tex_width, tex_height, 1};
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = tiling;
    image_create_info.usage = usage;
    image_create_info.flags = 0;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    
    VkMemoryAllocateInfo mem_alloc = {};
    mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_alloc.pNext = NULL;
    mem_alloc.allocationSize = 0;
    mem_alloc.memoryTypeIndex = 0;

    result = vkCreateImage(m_device, &image_create_info, NULL, &tex_obj->image);
    VK_CHECK_RESULT(result);

    vkGetImageMemoryRequirements(m_device, tex_obj->image, &m_mem_reqs);

    mem_alloc.allocationSize = m_mem_reqs.size;
    pass = memory_type_from_properties(m_mem_reqs.memoryTypeBits,
                                       required_props,
                                       &mem_alloc.memoryTypeIndex);

    /* allocate memory */
    result = vkAllocateMemory(m_device, &mem_alloc, NULL, &tex_obj->mem);
    VK_CHECK_RESULT(result);

    /* bind memory */
    result = vkBindImageMemory(m_device, tex_obj->image, tex_obj->mem, 0);
    VK_CHECK_RESULT(result);

    if (required_props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        VkImageSubresource subres = {};
        subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subres.mipLevel = 0;
        subres.arrayLayer = 0;
        VkSubresourceLayout layout;
        void *data;
        int32_t x, y;

        vkGetImageSubresourceLayout(m_device, tex_obj->image, &subres,
                                    &layout);

        result = vkMapMemory(m_device, tex_obj->mem, 0,
                             mem_alloc.allocationSize, 0, &data);
        VK_CHECK_RESULT(result);

        // Tile the texture over tex_height and tex_width
        for (y = 0; y < tex_height; y++) {
            uint32_t *row = (uint32_t *)((char *)data + layout.rowPitch * y);
            for (x = 0; x < tex_width; x++)
                row[x] = tex_colors[(x & 1) ^ (y & 1)];
        }

        vkUnmapMemory(m_device, tex_obj->mem);
    }

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo cmdAllocInfo = {};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = m_cmd_pool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &cmd);

    VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &cmdBeginInfo);
    
    // Initial transition to shader-readable layout
    set_image_layout(cmd, tex_obj->image,
                     VK_IMAGE_ASPECT_COLOR_BIT,
                     VK_IMAGE_LAYOUT_UNDEFINED,   // Initial layout
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     0, // No previous access
                     VK_PIPELINE_STAGE_HOST_BIT, // Host stage
                     VK_ACCESS_SHADER_READ_BIT,  // Shader read
                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    
    // Submit the command buffer to apply the transition
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    vkQueueSubmit(m_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);  // Wait for completion

    vkFreeCommandBuffers(m_device, m_cmd_pool, 1, &cmd);
}


void DynamicTextureWindow::prepare_textures()
{
    VkResult result;
    const VkFormat tex_format =  VK_FORMAT_R32G32B32A32_SFLOAT;
    const float tex_colors[DEMO_TEXTURE_COUNT][2*4] = {
        // B G R A     B G R A
        {0.F, 0.F, 1.F, 0.F, // Red, Green
         0.F, 1.F, 0.F, 0.F }
    };

    // Query if image supports texture format
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(m_gpu, tex_format, &props);

    for (int i = 0; i < DEMO_TEXTURE_COUNT; i++) {
        if ((props.linearTilingFeatures &
             VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) &&
            !m_use_staging_buffer) {
            // Device can texture using linear textures
            prepare_texture_image(
                tex_colors[i], &m_textures[i], VK_IMAGE_TILING_LINEAR,
                VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        } else if (props.optimalTilingFeatures &
                   VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            Fl::fatal("Staging buffer path not implemented in this demo");
        } else {
            /* Can't support VK_FORMAT_B8G8R8A8_UNORM !? */
            Fl::fatal("No support for B8G8R8A8_UNORM as texture image format");
        }
        
        VkSamplerCreateInfo sampler_info = {};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.pNext = NULL;
        sampler_info.magFilter = VK_FILTER_NEAREST;
        sampler_info.minFilter = VK_FILTER_NEAREST;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.anisotropyEnable = VK_FALSE;
        sampler_info.maxAnisotropy = 1;
        sampler_info.compareOp = VK_COMPARE_OP_NEVER;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 0.0f;
        sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        sampler_info.unnormalizedCoordinates = VK_FALSE;

        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.pNext = NULL;
        view_info.image = m_textures[i].image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = tex_format;
        view_info.components =
            {
                VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
                VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A,
            };
        view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        view_info.flags = 0;

        result = vkCreateSampler(m_device, &sampler_info, NULL,
                                 &m_textures[i].sampler);
        VK_CHECK_RESULT(result);

        result = vkCreateImageView(m_device, &view_info, NULL, &m_textures[i].view);
        VK_CHECK_RESULT(result);
    }
}

void DynamicTextureWindow::update_texture()
{
    
    VkCommandBuffer update_cmd;
    VkCommandBufferAllocateInfo cmdAllocInfo = {};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = m_cmd_pool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;
    vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &update_cmd);

    VkCommandBufferBeginInfo cmdBeginInfo = {};
    cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(update_cmd, &cmdBeginInfo);

    // Transition to GENERAL for CPU writes
    set_image_layout(update_cmd, m_textures[0].image, VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_HOST_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT);

    vkEndCommandBuffer(update_cmd);
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &update_cmd;
    vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);  // Synchronize before CPU write

    void* data;
    vkMapMemory(m_device, m_textures[0].mem, 0, m_mem_reqs.size, 0, &data);
    
    float* pixels = (float*)data;
    uint8_t intensity = (frame_counter++ % 255);
    pixels[0] = static_cast<float>(intensity) / 255.F;        // Red
    pixels[1] = 0;         // Green
    pixels[2] = 0;              // Blue
    pixels[3] = 1;// Alpha
    
    pixels[4] = 0;         // Red
    pixels[5] = static_cast<float>(intensity) / 255.F;        // Green
    pixels[6] = 0;              // Blue
    pixels[7] = 1;// Alpha
    
    vkUnmapMemory(m_device, m_textures[0].mem);

    // Reallocate command buffer for second transition
    vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &update_cmd);
    vkBeginCommandBuffer(update_cmd, &cmdBeginInfo);

    // Transition back to SHADER_READ_ONLY_OPTIMAL
    set_image_layout(update_cmd, m_textures[0].image, VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_ACCESS_HOST_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                    VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    vkEndCommandBuffer(update_cmd);
    submitInfo.pCommandBuffers = &update_cmd;
    vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);  // Synchronize before rendering

    vkFreeCommandBuffers(m_device, m_cmd_pool, 1, &update_cmd);
}


void DynamicTextureWindow::prepare_vertices()
{
    VkResult result;

    // clang-format off
    struct Vertex
    {
        float x, y, z;  // 3D position
        float u, v;      // UV coordinates
    };

    std::vector<Vertex> vertices(sides);
    float z = 0.5F;
    for (int j=0; j<sides; j++) {
        double ang = j*2*M_PI/sides;
        float x = cos(ang);
        float y = sin(ang);
        vertices[j].x = x;
        vertices[j].y = y;
        vertices[j].z = z;
        z += std::min(j / (float)(sides - 1), 1.F);
        vertices[j].u = x / 2 + 1;
        vertices[j].v = y / 2 + 1;
    }
    
    // clang-format on
    VkBufferCreateInfo buf_info = {};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.pNext = NULL;
    buf_info.size = sizeof(Vertex) * vertices.size();
    buf_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; 
    buf_info.flags = 0;

    result = vkCreateBuffer(m_device, &buf_info, NULL, &m_vertices.buf);
    VK_CHECK_RESULT(result);
    
    // Use a local variable instead of overwriting m_mem_reqs
    VkMemoryRequirements vertex_mem_reqs;
    vkGetBufferMemoryRequirements(m_device, m_vertices.buf, &vertex_mem_reqs);
    
    VkMemoryAllocateInfo mem_alloc = {};
    mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mem_alloc.pNext = NULL;
    mem_alloc.allocationSize = vertex_mem_reqs.size;
    mem_alloc.memoryTypeIndex = 0;
    
    bool pass;
    void *data;
    
    memory_type_from_properties(vertex_mem_reqs.memoryTypeBits,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                &mem_alloc.memoryTypeIndex);

    result = vkAllocateMemory(m_device, &mem_alloc, NULL, &m_vertices.mem);
    VK_CHECK_RESULT(result);

    result = vkMapMemory(m_device, m_vertices.mem, 0,
                         mem_alloc.allocationSize, 0, &data);
    VK_CHECK_RESULT(result);

    memcpy(data, vertices.data(), sizeof(Vertex) * vertices.size());

    vkUnmapMemory(m_device, m_vertices.mem);

    result = vkBindBufferMemory(m_device, m_vertices.buf, m_vertices.mem, 0);
    VK_CHECK_RESULT(result);

    m_vertices.vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    m_vertices.vi.pNext = NULL;
    m_vertices.vi.vertexBindingDescriptionCount = 1;
    m_vertices.vi.pVertexBindingDescriptions = m_vertices.vi_bindings;
    m_vertices.vi.vertexAttributeDescriptionCount = 2;
    m_vertices.vi.pVertexAttributeDescriptions = m_vertices.vi_attrs;

    m_vertices.vi_bindings[0].binding = VERTEX_BUFFER_BIND_ID;
    m_vertices.vi_bindings[0].stride = sizeof(vertices[0]);
    m_vertices.vi_bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    m_vertices.vi_attrs[0].binding = VERTEX_BUFFER_BIND_ID;
    m_vertices.vi_attrs[0].location = 0;
    m_vertices.vi_attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    m_vertices.vi_attrs[0].offset = 0;

    m_vertices.vi_attrs[1].binding = VERTEX_BUFFER_BIND_ID;
    m_vertices.vi_attrs[1].location = 1;
    m_vertices.vi_attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    m_vertices.vi_attrs[1].offset = sizeof(float) * 3;
}

// m_format, m_depth (optionally) -> creates m_renderPass
void DynamicTextureWindow::prepare_render_pass() 
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

VkShaderModule DynamicTextureWindow::prepare_vs() {
    if (m_vert_shader_module != VK_NULL_HANDLE)
        return m_vert_shader_module;
    
    // Example GLSL vertex shader
    std::string vertex_shader_glsl = R"(
        #version 450
        layout(location = 0) in vec3 inPos;
        layout(location = 1) in vec2 inTexCoord;
        layout(location = 0) out vec2 outTexCoord;
        void main() {
            gl_Position = vec4(inPos, 1.0);
            outTexCoord = inTexCoord;
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

VkShaderModule DynamicTextureWindow::prepare_fs() {
    if (m_frag_shader_module != VK_NULL_HANDLE)
        return m_frag_shader_module;
    
    // Example GLSL vertex shader
    std::string frag_shader_glsl = R"(
        #version 450

        // Input from vertex shader
        layout(location = 0) in vec2 inTexCoord;

        // Output color
        layout(location = 0) out vec4 outColor;

        // Texture sampler (bound via descriptor set)
        layout(binding = 0) uniform sampler2D textureSampler;

        void main() {
            outColor = texture(textureSampler, inTexCoord);
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

void DynamicTextureWindow::prepare_pipeline() {
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

}


void DynamicTextureWindow::prepare_descriptor_pool() {
    VkDescriptorPoolSize type_count = {};
    type_count.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    type_count.descriptorCount = DEMO_TEXTURE_COUNT;
    
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

void DynamicTextureWindow::prepare_descriptor_set() {
    VkDescriptorImageInfo tex_descs[DEMO_TEXTURE_COUNT];
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

    memset(&tex_descs, 0, sizeof(tex_descs));
    for (i = 0; i < DEMO_TEXTURE_COUNT; i++) {
        tex_descs[i].sampler = m_textures[i].sampler;
        tex_descs[i].imageView = m_textures[i].view;
        tex_descs[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_desc_set;
    write.descriptorCount = DEMO_TEXTURE_COUNT;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = tex_descs;

    vkUpdateDescriptorSets(m_device, 1, &write, 0, NULL);
}

void DynamicTextureWindow::prepare()
{
    prepare_textures();
    prepare_vertices();
    prepare_descriptor_layout();
    prepare_render_pass();
    prepare_pipeline();
    prepare_descriptor_pool();
    prepare_descriptor_set();

    Fl::add_timeout(1.0/60.0, (Fl_Timeout_Handler)texture_cb, this);
}

void DynamicTextureWindow::vk_draw_begin()
{
    // Background color
    m_clearColor = { 0.0, 0.0, 1.0, 1.0 };
    
    Fl_Vk_Window::vk_draw_begin();
}

void DynamicTextureWindow::draw() {
    
    update_texture();

    // Draw the triangle
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(m_draw_cmd, VERTEX_BUFFER_BIND_ID, 1,
                           &m_vertices.buf, offsets);

    vkCmdDraw(m_draw_cmd, sides, 1, 0, 0);

    Fl_Window::draw();
}

void DynamicTextureWindow::destroy_resources() {
    if (m_vertices.buf != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_vertices.buf, NULL);
        m_vertices.buf = VK_NULL_HANDLE;
    }

    if (m_vertices.mem != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_vertices.mem, NULL);
        m_vertices.mem = VK_NULL_HANDLE;
    }
    
    for (uint32_t i = 0; i < DEMO_TEXTURE_COUNT; i++) {
        if (m_textures[i].view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, m_textures[i].view, NULL);
            m_textures[i].view = VK_NULL_HANDLE;
        }
        if (m_textures[i].image != VK_NULL_HANDLE) {
            vkDestroyImage(m_device, m_textures[i].image, NULL);
            m_textures[i].image = VK_NULL_HANDLE;
        }
        if (m_textures[i].mem != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, m_textures[i].mem, NULL);
            m_textures[i].mem = VK_NULL_HANDLE;
        }
        if (m_textures[i].sampler != VK_NULL_HANDLE) {
            vkDestroySampler(m_device, m_textures[i].sampler, NULL);
            m_textures[i].sampler = VK_NULL_HANDLE;
        }
    }
}


void DynamicTextureWindow::prepare_descriptor_layout() {
    VkDescriptorSetLayoutBinding layout_binding = {};
    layout_binding.binding = 0;
    layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    layout_binding.descriptorCount = DEMO_TEXTURE_COUNT;
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
class DynamicTextureWindow : public Fl_Box {
public:
  int sides;
  DynamicTextureWindow(int x,int y,int w,int h,const char *l=0)
    :Fl_Box(FL_DOWN_BOX,x,y,w,h,l){
      label("This demo does\nnot work without Vulkan");
  }
};

#endif

// when you change the data, as in this callback, you must call redraw():
void sides_cb(Fl_Widget *o, void *p) {
  DynamicTextureWindow *sw = (DynamicTextureWindow *)p;
  sw->sides = int(((Fl_Slider *)o)->value());
  sw->prepare_vertices();
  sw->redraw();
}

int main(int argc, char **argv) {
    Fl::use_high_res_VK(1);

#ifdef _WIN32
    char* term = fl_getenv("TERM");
    if (!term || strlen(term) == 0)
    {
        BOOL ok = AttachConsole(ATTACH_PARENT_PROCESS);
        if (ok)
        {
            freopen("conout$", "w", stdout);
            freopen("conout$", "w", stderr);
        }
    }
#endif
  
#if 1
    Fl_Window window(300, 330);
  
// the shape window could be it's own window, but here we make it
// a child window:
        
    DynamicTextureWindow sw(10, 10, 280, 280);

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
#else
    DynamicTextureWindow sw(300, 330, "VK Window");
    sw.resizable(&sw);
    sw.show();
#endif
    
    //Fl::add_timeout(0.05, (Fl_Timeout_Handler)depth_stencil_cb, &sw);
        
    return Fl::run();
}
