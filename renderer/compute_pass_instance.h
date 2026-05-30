#pragma once

#include <cstdint>

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/descriptor_set/descriptor_set.h"

class ComputePass;
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
    void set_storage_texture(uint32_t binding, VulkanTexture2D& texture_2d);

    void set_cubemap(uint32_t binding, Cubemap& cubemap);
    void set_storage_cubemap(uint32_t binding, Cubemap& cubemap);
    void set_storage_image_view(uint32_t binding, const VulkanImageView& image_view);
    

    void bind(VulkanCommandBuffer& command_buffer);

private:
    ComputePass* m_compute_pass = nullptr;
    DescriptorSet m_descriptor_set;
};
