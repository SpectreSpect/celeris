#pragma once

#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"

class VulkanDevice;

class VulkanSemaphore {
public:
    _XCLASS_NAME(VulkanSemaphore);

    explicit VulkanSemaphore(const VulkanDevice& device);

    ~VulkanSemaphore() noexcept;
    void destroy() noexcept;

    VulkanSemaphore(const VulkanSemaphore&) = delete;
    VulkanSemaphore& operator=(const VulkanSemaphore&) = delete;

    VulkanSemaphore(VulkanSemaphore&& other) noexcept;
    VulkanSemaphore& operator=(VulkanSemaphore&& other) noexcept;

    VkSemaphore handle() const noexcept;

    static std::vector<VulkanSemaphore> create_semaphores(const VulkanDevice& device, size_t count_semaphores);

private:
    VkSemaphore m_semaphore = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
};
