#include "pipeline.h"
#include "../vulkan_command_buffer.h"

Pipeline::Pipeline(Pipeline&& other) noexcept
    :   m_pipeline(std::exchange(other.m_pipeline, VK_NULL_HANDLE)),
        m_device(std::exchange(other.m_device, VK_NULL_HANDLE)) {}

Pipeline& Pipeline::operator=(Pipeline&& other) noexcept {
    if (this != &other) {
        destroy();

        m_pipeline = std::exchange(other.m_pipeline, VK_NULL_HANDLE);
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    }

    return *this;
}

Pipeline::~Pipeline() noexcept {
    destroy();
}

void Pipeline::destroy() noexcept {
    if (m_device != VK_NULL_HANDLE && m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
    }

    m_device = VK_NULL_HANDLE;
    m_pipeline = VK_NULL_HANDLE;
}


VkPipeline Pipeline::handle() const noexcept {
    return m_pipeline;
}

VkDevice Pipeline::device() const noexcept {
    return m_device;
}

VkPipelineLayout Pipeline::layout() const noexcept {
    return m_layout;
}

void Pipeline::set_pipeline(VkDevice device, VkPipeline pipeline, VkPipelineLayout layout) {
    LOG_METHOD();

    logger.check(m_device == VK_NULL_HANDLE, "Pipeline device has already been initialized");
    logger.check(m_pipeline == VK_NULL_HANDLE, "Pipeline has already been initialized");
    logger.check(device != VK_NULL_HANDLE, "Cannot set null device");
    logger.check(pipeline != VK_NULL_HANDLE, "Cannot set null pipeline");
    logger.check(layout != VK_NULL_HANDLE, "Cannot set null pipeline layout");

    m_device = device;
    m_pipeline = pipeline;
    m_layout = layout;
}

void Pipeline::bind(VulkanCommandBuffer& command_buffer) const {
    LOG_METHOD();

    logger.check(m_pipeline != VK_NULL_HANDLE, "Pipeline is not initialized");
    logger.check(command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initialized");

    vkCmdBindPipeline(command_buffer.handle(), get_bind_point(), m_pipeline);
}