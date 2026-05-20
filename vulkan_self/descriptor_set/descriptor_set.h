#pragma once

#include <vulkan/vulkan.h>

#include "../logger/logger_header.h"

class VulkanDevice;
class DescriptorPool;
class DescriptorSetLayout;
class VulkanBuffer;
class VulkanCommandBuffer;
class VulkanPipelineLayout;

class DescriptorSet {
public:
    _XCLASS_NAME(DescriptorSet);

    explicit DescriptorSet(VkDevice device, VkDescriptorSet descriptor_set) noexcept
        : m_device(device), m_descriptor_set(descriptor_set) {};
    
    void write_buffer(uint32_t binding, VulkanBuffer& buffer, VkDescriptorType descriptor_type);
    
    void write_uniform_buffer(uint32_t binding, VulkanBuffer& buffer);
    void write_storage_buffer(uint32_t binding, VulkanBuffer& buffer);

    void bind(VulkanCommandBuffer& command_buffer, VulkanPipelineLayout& pipeline_layout);

private:
    VkDescriptorSet m_descriptor_set = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
};