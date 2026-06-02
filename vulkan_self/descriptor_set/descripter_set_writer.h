#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

#include "../logger/logger_header.h"

class VulkanBuffer;
class VulkanTexture2D;
class VulkanCommandBuffer;
class Pipeline;

class DescriptorSetWriter {
public:
    _XCLASS_NAME(DescriptorSetWriter);

    DescriptorSetWriter() = default;
    ~DescriptorSetWriter() noexcept = default;

    DescriptorSetWriter(const DescriptorSetWriter&) = delete;
    DescriptorSetWriter& operator=(const DescriptorSetWriter&) = delete;

    DescriptorSetWriter(DescriptorSetWriter&&) noexcept = default;
    DescriptorSetWriter& operator=(DescriptorSetWriter&&) noexcept = default;

    DescriptorSetWriter& write_buffer(uint32_t binding, const VulkanBuffer& buffer, VkDescriptorType descriptor_type);
    
    DescriptorSetWriter& write_uniform_buffer(uint32_t binding, const VulkanBuffer& buffer);
    DescriptorSetWriter& write_storage_buffer(uint32_t binding, const VulkanBuffer& buffer);
    DescriptorSetWriter& write_texture(uint32_t binding, const VulkanTexture2D& texture);

    void clear_writes();
    void push_descriptor_set(VulkanCommandBuffer& command_buffer, const Pipeline& pipeline, uint32_t set_index = 0) const;
    void push_descriptor_set_and_clear(VulkanCommandBuffer& command_buffer, const Pipeline& pipeline, uint32_t set_index = 0);

private:
    struct PendingWrite {
        uint32_t binding;
        VkDescriptorType descriptor_type;
        uint32_t info_index;
        bool is_buffer;
    };

private:
    std::vector<VkDescriptorBufferInfo> m_buffer_infos;
    std::vector<VkDescriptorImageInfo> m_image_infos;
    std::vector<PendingWrite> m_pending_writes;
};
