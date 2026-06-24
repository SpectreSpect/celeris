#include "vulkan_submit_context.h"
#include "vulkan_device.h"

SubmitAndWaitScope::SubmitAndWaitScope(VulkanSubmitContext& submit_context) 
    :   m_submit_context(submit_context)
{
    m_submit_context.m_command_buffer.begin();
}

SubmitAndWaitScope::~SubmitAndWaitScope() noexcept {
    m_submit_context.m_command_buffer.end();
    m_submit_context.submit_and_wait();
}

const VulkanCommandBuffer& SubmitAndWaitScope::command_buffer() const noexcept {
    return m_submit_context.command_buffer();
}

VulkanCommandBuffer& SubmitAndWaitScope::command_buffer() noexcept {
    return m_submit_context.command_buffer();
}

VulkanSubmitContext::VulkanSubmitContext(
    const VulkanDevice& device,
    VulkanQueue& queue)
    :   m_queue(&queue),
        m_command_pool(device, queue),
        m_command_buffer(device, m_command_pool),
        m_fence(device) {}

const VulkanQueue& VulkanSubmitContext::queue() const {
    LOG_METHOD();

    logger.check(m_queue != nullptr, "Queue pointer specify to null");

    return *m_queue;
}

VulkanQueue& VulkanSubmitContext::queue() {
    LOG_METHOD();

    logger.check(m_queue != nullptr, "Queue pointer specify to null");

    return *m_queue;
}

const VulkanCommandPool& VulkanSubmitContext::command_pool() const noexcept {
    return m_command_pool;
}

VulkanCommandPool& VulkanSubmitContext::command_pool() noexcept {
    return m_command_pool;
}

const VulkanCommandBuffer& VulkanSubmitContext::command_buffer() const noexcept {
    return m_command_buffer;
}

VulkanCommandBuffer& VulkanSubmitContext::command_buffer() noexcept {
    return m_command_buffer;
}

const VulkanFence& VulkanSubmitContext::fence() const noexcept {
    return m_fence;
}

VulkanFence& VulkanSubmitContext::fence() noexcept {
    return m_fence;
}

CommandBufferScope VulkanSubmitContext::record_commands() {
    LOG_METHOD();
    
    return m_command_buffer.begin_scope();
}

void VulkanSubmitContext::submit_and_wait() {
    LOG_METHOD();

    logger.check(m_queue != nullptr, "Queue pointer specify to null");

    m_fence.reset();
    m_queue->submit(m_command_buffer, &m_fence);
    m_fence.wait();
    m_command_buffer.reset();
}

SubmitAndWaitScope VulkanSubmitContext::submit_and_wait_scope() {
    LOG_METHOD();
    return SubmitAndWaitScope(*this);
}
