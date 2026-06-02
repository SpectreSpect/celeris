#include "compute_pass_instance.h"

#include "../vulkan_self/descriptor_set/descriptor_pool.h"
#include "../vulkan_self/compute/compute_pass.h"
#include "../vulkan_self/vulkan_command_buffer.h"
#include "../vulkan_self/vulkan_buffer.h"
#include "../vulkan_self/image/vulkan_texture_2d.h"

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

void ComputePassInstance::bind(VulkanCommandBuffer& command_buffer) {
    LOG_METHOD();

    logger.check(m_compute_pass != nullptr, "Compute pass pointer specify to null");

    m_compute_pass->pipeline().bind(command_buffer);
    m_descriptor_set.bind(command_buffer, m_compute_pass->pipeline(), 0);
}

std::vector<ComputePassInstance> ComputePassInstance::create_compute_pass_instances(DescriptorPool& pool, ComputePass& pass, uint32_t count) {
    LOG_NAMED("ComputePassInstance");

    logger.check(count > 0, "Attempt to create zero instances");
    logger.check(pool.handle() != VK_NULL_HANDLE, "Descriptor pool is not initialized");
    logger.check(pass.pipeline().handle() != VK_NULL_HANDLE, "Compute pass pipeline is not initialized");

    std::vector<ComputePassInstance> instances;
    instances.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        instances.emplace_back(pool, pass);
    }

    return instances;
}