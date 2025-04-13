#ifndef FL_Vk_Utils_H
#define FL_Vk_Utils_H

#include "Fl_Export.H"

#include <FL/vk.h>
#include <shaderc/shaderc.hpp>

#include <string>
#include <vector>

// Function used to compile a glsl shader to SPRIV (needs shaderc)
FL_EXPORT
std::vector<uint32_t> compile_glsl_to_spirv(
    const std::string& source_code,
    shaderc_shader_kind shader_kind,
    const std::string& filename = "shader.glsl",
    bool optimize = false);

// Function to create a shader module from SPIRV byte code
FL_EXPORT
VkShaderModule
create_shader_module(VkDevice device,
                     const std::vector<uint32_t>& spirv_code);

// Command buffer functions
FL_EXPORT VkCommandBuffer beginSingleTimeCommands(VkDevice device,
                                                  VkCommandPool commandPool);

FL_EXPORT void endSingleTimeCommands(VkCommandBuffer commandBuffer,
                                     VkDevice device,
                                     VkCommandPool commandPool,
                                     VkQueue queue);

// Image functions
FL_EXPORT VkImage createImage(VkDevice device,
                              VkImageType imageType,
                              uint32_t width, uint32_t height, uint32_t depth,
                              VkFormat format,
                              VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
                              VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT |
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT);

FL_EXPORT bool memory_type_from_properties(VkPhysicalDevice gpu,
                                           uint32_t typeBits, VkFlags requirements_mask,
                                           uint32_t *typeIndex);

FL_EXPORT VkDeviceMemory allocateAndBindImageMemory(VkDevice device,
                                                    VkPhysicalDevice gpu,
                                                    VkImage image,
                                                    VkFlags requirementsMask = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

FL_EXPORT void createBuffer(VkDevice device,
                            VkPhysicalDevice gpu,
                            VkDeviceSize size, VkBufferUsageFlags usage,
                            VkMemoryPropertyFlags properties, VkBuffer& buffer,
                            VkDeviceMemory& bufferMemory);

FL_EXPORT void set_image_layout(VkDevice device,
                                VkCommandPool commandPool,
                                VkQueue queue,
                                VkImage image,
                                VkImageAspectFlags aspectMask,
                                VkImageLayout old_image_layout,
                                VkImageLayout new_image_layout,
                                VkAccessFlags srcAccessMask,
                                VkPipelineStageFlags srcStageMask,
                                VkAccessFlags dstAccessMask,
                                VkPipelineStageFlags dstStageMask);

FL_EXPORT void transitionImageLayout(VkDevice device,
                                     VkCommandPool commandPool,
                                     VkQueue queue,
                                     VkImage image,
                                     VkImageLayout oldLayout,
                                     VkImageLayout newLayout);

VkImageView createImageView(VkDevice device,
                            VkImage image, VkFormat format,
                            VkImageType imageType);

VkSampler createSampler(VkDevice device,
                        VkFilter magFilter = VK_FILTER_LINEAR,
                        VkFilter minFilter = VK_FILTER_LINEAR,
                        VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                        VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                        VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
#endif
