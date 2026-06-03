#include "pass_writer.h"
#include "../../vulkan_device.h"
#include "../pipeline_pass.h"
#include "../../pipeline/pipeline.h"

PassWriter::PassWriter(VulkanDevice& device, PipelinePass& pass)
    :   PassObject(pass),
        m_writer(device)
{
    LOG_METHOD();
    
    logger.check(device.handle() != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(pass.pipeline().handle() != VK_NULL_HANDLE, "Pipeline is not initialized");
}

void PassWriter::set_uniform_buffer(uint32_t binding, VulkanBuffer& uniform_buffer) {
    LOG_METHOD();
    m_writer.write_uniform_buffer(binding, uniform_buffer);
}

void PassWriter::set_storage_buffer(uint32_t binding, VulkanBuffer& storage_buffer) {
    LOG_METHOD();
    m_writer.write_storage_buffer(binding, storage_buffer);
}

void PassWriter::set_texture(uint32_t binding, VulkanTexture2D& texture_2d) {
    LOG_METHOD();
    m_writer.write_texture(binding, texture_2d);
}

void PassWriter::bind_description_object(VulkanCommandBuffer& command_buffer) {
    LOG_METHOD();
    m_writer.push_descriptor_set_and_clear(command_buffer, pipepline_pass().pipeline(), 0);
}
