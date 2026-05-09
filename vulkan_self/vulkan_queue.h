#pragma once

#include <cstdint>
#include <utility>
#include <span>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vulkan_queue_types.h"
#include "logger/logger_header.h"

class VulkanDevice;
class VulkanCommandBuffer;
class VulkanFence;
class VulkanSwapchain;

class VulkanQueue {
public:
    _XCLASS_NAME(VulkanQueue);

    explicit VulkanQueue(const VulkanDevice& device, QueueLocation location, VulkanQueueType type);

    VulkanQueue(const VulkanQueue&) = delete;
    VulkanQueue& operator=(const VulkanQueue&) = delete;

    VulkanQueue(VulkanQueue&& other) noexcept;
    VulkanQueue& operator=(VulkanQueue&& other) noexcept;

    VkQueue handle() const noexcept;
    QueueLocation location() const noexcept;
    VulkanQueueType type() const noexcept;
    QueueFamilyInfo family_info() const noexcept;

    void submit(
        std::span<const VkSemaphore> wait_semaphores,
        std::span<const VkPipelineStageFlags> wait_stages,
        std::span<VulkanCommandBuffer> command_buffers,
        std::span<VkSemaphore> signal_semaphores,
        VulkanFence& fence
    );

    void submit(
        VkSemaphore wait_semaphore,
        VkPipelineStageFlags wait_stage,
        VulkanCommandBuffer& command_buffer,
        VkSemaphore signal_semaphore,
        VulkanFence& fence
    );

    VkResult present(
        std::span<const VkSemaphore> wait_semaphores,
        std::span<const VulkanSwapchain> swapchains,
        std::span<const uint32_t> image_indices
    );

    VkResult present(
        VkSemaphore wait_semaphore,
        const VulkanSwapchain& swapchain,
        uint32_t image_index
    );

private:
    VkQueue m_queue = VK_NULL_HANDLE;
    QueueLocation m_location{};
    VulkanQueueType m_type = VulkanQueueType::Graphics;
};
