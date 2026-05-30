#include "compute_pass_instance.h"

#include "../vulkan_self/descriptor_set/descriptor_pool.h"
#include "../vulkan_self/compute/compute_pass.h"
#include "../vulkan_self/vulkan_command_buffer.h"
#include "../vulkan_self/vulkan_buffer.h"
#include "../vulkan_self/image/vulkan_texture_2d.h"
#include "../vulkan_self/image/vulkan_image_view.h"

ComputePassInstance::ComputePassInstance(DescriptorPool& pool, ComputePass& pass) 
    :   m_compute_pass(&pass),
        m_descriptor_set(pool.allocate_set(pass.descriptor_set_layout())) {}

void ComputePassInstance::set_uniform_buffer(uint32_t binding, VulkanBuffer& uniform_buffer) {
    LOG_METHOD();
    m_descriptor_set.write_uniform_buffer(binding, uniform_buffer);
}

void ComputePassInstance::set_storage_buffer(uint32_t binding, VulkanBuffer& storage_buffer) {
    LOG_METHOD();
    m_descriptor_set.write_storage_buffer(binding, storage_buffer);
}

void ComputePassInstance::set_texture(uint32_t binding, VulkanTexture2D& texture_2d) {
    LOG_METHOD();
    m_descriptor_set.write_texture(binding, texture_2d);
}

void ComputePassInstance::set_storage_texture(uint32_t binding, VulkanTexture2D& texture_2d) {
    LOG_METHOD();
    m_descriptor_set.write_storage_texture(binding, texture_2d);
}

void ComputePassInstance::set_cubemap(uint32_t binding, Cubemap& cubemap) {
    LOG_METHOD();
    m_descriptor_set.write_cubemap(binding, cubemap);
}


void ComputePassInstance::set_storage_cubemap(uint32_t binding, Cubemap& cubemap) {
    LOG_METHOD();
    m_descriptor_set.write_storage_cubemap(binding, cubemap);
}

void ComputePassInstance::set_storage_image_view(uint32_t binding, const VulkanImageView& image_view) {
    LOG_METHOD();
    m_descriptor_set.write_storage_image_view(binding, image_view);
}

void ComputePassInstance::bind(VulkanCommandBuffer& command_buffer) {
    LOG_METHOD();

    logger.check(m_compute_pass != nullptr, "Compute pass pointer specify to null");

    m_compute_pass->pipeline().bind(command_buffer);
    m_descriptor_set.bind(command_buffer, m_compute_pass->pipeline(), 0);
}
