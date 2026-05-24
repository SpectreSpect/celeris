#include "vulkan_queue.h"

#include <vector>
#include <string>

#include "vulkan_device.h"
#include "vulkan_command_buffer.h"
#include "vulkan_fence.h"
#include "vulkan_swapchain.h"
#include "vulkan_semaphore.h"

#include "collect_handles_helper.h"

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
    std::span<const VulkanSemaphore> wait_semaphores,
    std::span<const VkPipelineStageFlags> wait_stages,
    std::span<VulkanCommandBuffer> command_buffers,
    std::span<VulkanSemaphore> signal_semaphores,
    VulkanFence* fence)
{
    LOG_METHOD();

    logger.check(m_queue != VK_NULL_HANDLE, "Queue has not been initialized");

    logger.check(wait_semaphores.size() == wait_stages.size())
        << "The number of simaphores (" << clr(std::to_string(wait_semaphores.size()), LoggerPalette::orange)
        << ") does not match the number of waiting stages "
        << "(" << clr(std::to_string(wait_stages.size()), LoggerPalette::orange) << ")";
    
    for (size_t i = 0; i < wait_semaphores.size(); i++) {
        logger.check(wait_semaphores[i].handle() != VK_NULL_HANDLE)
            << "Wait semaphore " << clr(std::to_string(i), LoggerPalette::blue) << " is not initialized";
    }

    for (size_t i = 0; i < command_buffers.size(); i++) {
        logger.check(command_buffers[i].handle() != VK_NULL_HANDLE)
            << "Command buffer " << clr(std::to_string(i), LoggerPalette::blue) << " is not initialized";
    }

    for (size_t i = 0; i < signal_semaphores.size(); i++) {
        logger.check(signal_semaphores[i].handle() != VK_NULL_HANDLE)
            << "Signal semaphore " << clr(std::to_string(i), LoggerPalette::blue) << " is not initialized";
    }

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    std::vector<VkSemaphore> wait_semaphore_handles = collect_handles(wait_semaphores);
    std::vector<VkCommandBuffer> command_buffer_handles = collect_handles(command_buffers);
    std::vector<VkSemaphore> signal_semaphore_handles = collect_handles(signal_semaphores);

    submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphore_handles.size());
    submit_info.pWaitSemaphores = wait_semaphore_handles.data();
    submit_info.pWaitDstStageMask = wait_stages.data();

    submit_info.commandBufferCount = static_cast<uint32_t>(command_buffer_handles.size());
    submit_info.pCommandBuffers = command_buffer_handles.data();

    submit_info.signalSemaphoreCount = static_cast<uint32_t>(signal_semaphore_handles.size());
    submit_info.pSignalSemaphores = signal_semaphore_handles.data();

    VkResult submit_result = vkQueueSubmit(
        m_queue,
        1,
        &submit_info,
        fence != nullptr ? fence->handle() : VK_NULL_HANDLE
    );

    logger.check(submit_result == VK_SUCCESS, "Failed to submit draw command buffer");
}

void VulkanQueue::submit(
    VulkanSemaphore* wait_semaphore,
    VkPipelineStageFlags wait_stage,
    VulkanCommandBuffer& command_buffer,
    VulkanSemaphore* signal_semaphore,
    VulkanFence* fence)
{
    std::span<const VulkanSemaphore> wait_semaphores(wait_semaphore, wait_semaphore != nullptr ? 1 : 0);
    std::span<const VkPipelineStageFlags> wait_stages(&wait_stage, wait_semaphore != nullptr ? 1 : 0);
    std::span<VulkanCommandBuffer> command_buffers = {&command_buffer, 1};
    std::span<VulkanSemaphore> signal_semaphores(signal_semaphore, signal_semaphore != nullptr ? 1 : 0);

    submit(
        wait_semaphores,
        wait_stages,
        command_buffers,
        signal_semaphores,
        fence
    );
}

void VulkanQueue::submit(
    VulkanCommandBuffer& command_buffer,
    VulkanFence* fence)
{
    submit(
        nullptr,
        0,
        command_buffer,
        nullptr,
        fence
    );
}

VkResult VulkanQueue::present(
    std::span<const VulkanSemaphore> wait_semaphores,
    std::span<const VulkanSwapchain> swapchains,
    std::span<const uint32_t> image_indices)
{
    LOG_METHOD();

    logger.check(m_queue != VK_NULL_HANDLE, "Queue has not been initialized");

    logger.check(m_type == VulkanQueueType::Present)
        << "The queue type must be '" << clr("Present", LoggerPalette::blue)
        << "', but it is '" << clr(queue_type_str(m_type), LoggerPalette::blue) << "'\n";
    
    logger.check(swapchains.size() == image_indices.size(), "The number of swapchains is not equal to the number of image_indices");

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    std::vector<VkSemaphore> wait_semaphore_handles = collect_handles(wait_semaphores);
    std::vector<VkSwapchainKHR> swapchain_handles = collect_handles(swapchains);

    present_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphore_handles.size());
    present_info.pWaitSemaphores = wait_semaphore_handles.data();
    
    present_info.swapchainCount = static_cast<uint32_t>(swapchain_handles.size());
    present_info.pSwapchains = swapchain_handles.data();
    present_info.pImageIndices = image_indices.data();

    VkResult present_result = vkQueuePresentKHR(m_queue, &present_info);

    logger.check(
        present_result == VK_SUCCESS ||
        present_result == VK_SUBOPTIMAL_KHR ||
        present_result == VK_ERROR_OUT_OF_DATE_KHR, 
        "Failed to present swapchain image"
    );

    return present_result;
}

VkResult VulkanQueue::present(
    VulkanSemaphore& wait_semaphore,
    const VulkanSwapchain& swapchain,
    uint32_t image_index)
{
    std::span<const VulkanSemaphore> wait_semaphores = {&wait_semaphore, 1};
    std::span<const VulkanSwapchain> swapchains = {&swapchain, 1};
    std::span<const uint32_t> image_indices = {&image_index, 1};

    return present(wait_semaphores, swapchains, image_indices);
}
