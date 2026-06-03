#include "pass_instance.h"

#include "../vulkan_self/descriptor_set/descriptor_pool.h"
#include "../vulkan_self/pipeline/pipeline_pass.h"
#include "../vulkan_self/vulkan_command_buffer.h"
#include "../vulkan_self/vulkan_buffer.h"
#include "../vulkan_self/image/vulkan_texture_2d.h"

PassInstance::PassInstance(PipelinePass& pass, DescriptorPool& pool) 
    :   PassObject(pass),
        m_descriptor_set(pool.allocate_set(pass.descriptor_set_layout())) {}

void PassInstance::set_uniform_buffer(uint32_t binding, VulkanBuffer& uniform_buffer) {
    LOG_METHOD();
    m_descriptor_set.write_uniform_buffer(binding, uniform_buffer);
}

void PassInstance::set_storage_buffer(uint32_t binding, VulkanBuffer& storage_buffer) {
    LOG_METHOD();
    m_descriptor_set.write_storage_buffer(binding, storage_buffer);
}


void PassInstance::set_texture(uint32_t binding, VulkanTexture2D& texture_2d) {
    LOG_METHOD();
    m_descriptor_set.write_texture(binding, texture_2d);
}

void PassInstance::bind_description_object(VulkanCommandBuffer& command_buffer) {
    LOG_METHOD();
    m_descriptor_set.bind(command_buffer, pipepline_pass().pipeline(), 0);
}
