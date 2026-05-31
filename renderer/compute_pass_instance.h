#pragma once

#include <cstdint>
#include <utility>

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/descriptor_set/descriptor_set.h"
#include "../vulkan_self/compute/compute_pass.h"

class DescriptorPool;
class VulkanCommandBuffer;
class VulkanBuffer;
class VulkanTexture2D;

class ComputePassInstance {
public:
    _XCLASS_NAME(ComputePassInstance);

    ComputePassInstance(DescriptorPool& pool, ComputePass& pass);

    void set_uniform_buffer(uint32_t binding, VulkanBuffer& uniform_buffer);
    void set_storage_buffer(uint32_t binding, VulkanBuffer& storage_buffer);
    void set_texture(uint32_t binding, VulkanTexture2D& texture_2d);
    
    template <class... Args>
    void push_constants(VulkanCommandBuffer& command_buffer, Args&&... args) const {
        LOG_METHOD();

        logger.check(m_compute_pass != nullptr, "Compute pass pointer specify to null");

        m_compute_pass->pipeline_layout().push_constants(command_buffer, std::forward<Args>(args)...);
    }

    void bind(VulkanCommandBuffer& command_buffer);

private:
    ComputePass* m_compute_pass = nullptr;
    DescriptorSet m_descriptor_set;
};
