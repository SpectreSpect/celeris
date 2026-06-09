#pragma once

#include <cstdint>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"
#include "vulkan_queue_types.h"

class VulkanDevice;
class VulkanQueue;

class VulkanCommandPool {
public:
    _XCLASS_NAME(VulkanCommandPool);

    explicit VulkanCommandPool(const VulkanDevice& device, QueueFamilyInfo queue_family_info);
    explicit VulkanCommandPool(const VulkanDevice& device, const VulkanQueue& queue);
    ~VulkanCommandPool() noexcept;
    void destroy() noexcept;

    VulkanCommandPool(const VulkanCommandPool&) = delete;
    VulkanCommandPool& operator=(const VulkanCommandPool&) = delete;

    VulkanCommandPool(VulkanCommandPool&& other) noexcept;
    VulkanCommandPool& operator=(VulkanCommandPool&& other) noexcept;

    VkCommandPool handle() const noexcept;
    QueueFamilyInfo queue_family_info() const noexcept;

private:
    VkCommandPool m_command_pool = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    QueueFamilyInfo m_queue_family_info{};
};
