#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../logger/logger_header.h"

class VulkanDevice;
class DescriptorPool;
class DescriptorSetLayout;
class VulkanBuffer;
class VulkanCommandBuffer;
class VulkanPipelineLayout;
class Pipeline;
class VulkanTexture2D;
class Cubemap;

class DescriptorSet {
public:
    _XCLASS_NAME(DescriptorSet);

    explicit DescriptorSet(VkDevice device, VkDescriptorSet descriptor_set) noexcept
        : m_device(device), m_descriptor_set(descriptor_set) {};
    
    void write_buffer(uint32_t binding, VulkanBuffer& buffer, VkDescriptorType descriptor_type);
    void write_uniform_buffer(uint32_t binding, VulkanBuffer& buffer);
    void write_storage_buffer(uint32_t binding, VulkanBuffer& buffer);

    void write_texture(uint32_t binding, const VulkanTexture2D& texture);
    void write_cubemap(uint32_t binding, const Cubemap& cubemap);
    void write_storage_cubemap(uint32_t binding, const Cubemap& cubemap);

    void bind(VulkanCommandBuffer& command_buffer, VkPipelineLayout pipeline_layout, VkPipelineBindPoint bind_point, uint32_t set_binding);
    void bind(VulkanCommandBuffer& command_buffer, VulkanPipelineLayout& pipeline_layout, VkPipelineBindPoint bind_point, uint32_t set_binding);
    void bind(VulkanCommandBuffer& command_buffer, Pipeline& pipeline, uint32_t set_binding);
    
private:
    VkDescriptorSet m_descriptor_set = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
};