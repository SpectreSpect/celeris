#include "vulkan_queue.h"

#include "vulkan_device.h"
#include "vulkan_command_buffer.h"
#include "vulkan_fence.h"

VulkanQueue::VulkanQueue(const VulkanDevice& device, QueueLocation location, VulkanQueueType type)
    : m_location(location), m_type(type)
{
    LOG_METHOD();

    logger.check(device.handle() != VK_NULL_HANDLE, "Device has not been initialized");

    vkGetDeviceQueue(device.handle(), location.family_index, location.queue_index, &m_queue);
}

VulkanQueue::VulkanQueue(VulkanQueue&& other) noexcept
    :   m_queue(std::exchange(other.m_queue, VK_NULL_HANDLE)) ,
        m_location(std::exchange(other.m_location, QueueLocation{})),
        m_type(std::exchange(other.m_type, VulkanQueueType::Graphics)) {}

VulkanQueue& VulkanQueue::operator=(VulkanQueue&& other) noexcept {
    if (this != &other) {
        m_queue = std::exchange(other.m_queue, VK_NULL_HANDLE);
        m_location = std::exchange(other.m_location, QueueLocation{});
        m_type = std::exchange(other.m_type, VulkanQueueType::Graphics);
    }

    return *this;
}

VkQueue VulkanQueue::handle() const noexcept {
    return m_queue;
}

QueueLocation VulkanQueue::location() const noexcept {
    return m_location;
}

VulkanQueueType VulkanQueue::type() const noexcept {
    return m_type;
}

QueueFamilyInfo VulkanQueue::family_info() const noexcept {
    return QueueFamilyInfo{
        .queue_family_index = m_location.family_index,
        .queue_family_type = m_type
    };
}

void VulkanQueue::submit(
    std::span<VkSemaphore> wait_semaphores,
    std::span<VkPipelineStageFlags> wait_stages,
    std::span<VulkanCommandBuffer> command_buffers,
    std::span<VkSemaphore> signal_semaphores,
    VulkanFence& fence)
{
    LOG_METHOD();

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    logger.check(wait_semaphores.size() == wait_stages.size())
        << "The number of simaphores (" << clr(std::to_string(wait_semaphores.size()), LoggerPalette::orange)
        << ") does not match the number of waiting stages "
        << "(" << clr(std::to_string(wait_stages.size()), LoggerPalette::orange) << ")";

    submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
    submit_info.pWaitSemaphores = wait_semaphores.data();
    submit_info.pWaitDstStageMask = wait_stages.data();

    std::vector<VkCommandBuffer> command_buffer_handles;
    command_buffer_handles.reserve(command_buffers.size());
    for (VulkanCommandBuffer& command_buffer : command_buffers) {
        command_buffer_handles.push_back(command_buffer.handle());
    }

    submit_info.commandBufferCount = command_buffer_handles.size();
    submit_info.pCommandBuffers = command_buffer_handles.data();

    submit_info.signalSemaphoreCount = signal_semaphores.size();
    submit_info.pSignalSemaphores = signal_semaphores.data();

    VkResult submit_result = vkQueueSubmit(
        m_queue,
        1,
        &submit_info,
        fence.handle()
    );

    logger.check(submit_result == VK_SUCCESS, "Failed to submit draw command buffer");
}

void VulkanQueue::submit(
    VkSemaphore wait_semaphore,
    VkPipelineStageFlags wait_stage,
    VulkanCommandBuffer& command_buffer,
    VkSemaphore signal_semaphore,
    VulkanFence& fence)
{
    std::span<VkSemaphore> wait_semaphores = {&wait_semaphore, 1};
    std::span<VkPipelineStageFlags> wait_stages = {&wait_stage, 1};
    std::span<VulkanCommandBuffer> command_buffers = {&command_buffer, 1};
    std::span<VkSemaphore> signal_semaphores = {&signal_semaphore, 1};
    submit(
        wait_semaphores,
        wait_stages,
        command_buffers,
        signal_semaphores,
        fence
    );
}
