#include "pass_instance.h"

#include "../../descriptor_set/descriptor_pool.h"
#include "../pipeline_pass.h"
#include "../../vulkan_command_buffer.h"
#include "../../vulkan_buffer.h"
#include "../../image/vulkan_texture_2d.h"
#include "../../image/cubemap.h"
#include "../../image/vulkan_image_view.h"

PassInstance::PassInstance(PipelinePass& pass, DescriptorPool& pool, uint32_t instance_set_id) 
    :   PassObject(pass),
        m_descriptor_set(pool.allocate_set(pass.descriptor_set_layout())),
        m_instance_set_id(instance_set_id) {}

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

void PassInstance::set_storage_texture(uint32_t binding, VulkanTexture2D& texture_2d) {
    LOG_METHOD();
    m_descriptor_set.write_storage_texture(binding, texture_2d);
}

void PassInstance::set_cubemap(uint32_t binding, Cubemap& cubemap) {
    LOG_METHOD();
    m_descriptor_set.write_cubemap(binding, cubemap);
}

void PassInstance::set_storage_cubemap(uint32_t binding, Cubemap& cubemap) {
    LOG_METHOD();
    m_descriptor_set.write_storage_cubemap(binding, cubemap);
}

void PassInstance::set_storage_image_view(uint32_t binding, const VulkanImageView& image_view) {
    LOG_METHOD();
    m_descriptor_set.write_storage_image_view(binding, image_view);
}

void PassInstance::bind_description_object(VulkanCommandBuffer& command_buffer) {
    LOG_METHOD();
    m_descriptor_set.bind(command_buffer, pipepline_pass().pipeline(), m_instance_set_id);
}

DescriptorSet& PassInstance::descripter_set() noexcept {
    return m_descriptor_set;
}

const DescriptorSet& PassInstance::descripter_set() const noexcept {
    return m_descriptor_set;
}
