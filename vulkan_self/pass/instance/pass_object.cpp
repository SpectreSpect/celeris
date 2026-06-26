#include "pass_object.h"

#include "../../vulkan_buffer.h"
#include "../../image/vulkan_texture_2d.h"
#include "../../vulkan_command_buffer.h"

PassObject::PassObject(PipelinePass& pass) : m_pipeline_pass(&pass) {
    LOG_METHOD();

    logger.check(pass.pipeline_layout().handle() != VK_NULL_HANDLE, "Pipeline layout is not initialized");
}

void PassObject::bind(VulkanCommandBuffer& command_buffer) {
    LOG_METHOD();

    logger.check(command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initialized");
    logger.check(m_pipeline_pass != nullptr, "Pipepline pass pointer specify to null");

    m_pipeline_pass->bind(command_buffer);
    bind_description_object(command_buffer);
}

PipelinePass& PassObject::pipepline_pass() {
    LOG_METHOD();

    logger.check(m_pipeline_pass != nullptr, "Pipepline pass pointer specify to null");

    return *m_pipeline_pass;
}

const PipelinePass& PassObject::pipepline_pass() const {
    LOG_METHOD();

    logger.check(m_pipeline_pass != nullptr, "Pipepline pass pointer specify to null");

    return *m_pipeline_pass;
}
