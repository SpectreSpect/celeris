#pragma once

#include <utility>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "window.h"
#include "vulkan_instance.h"
#include "logger/logger_header.h"

class VulkanSurface {
public:
    _XCLASS_NAME(VulkanSurface);

    explicit VulkanSurface(const VulkanInstance& instance, const Window& window);
    ~VulkanSurface();
    void destroy();

    VulkanSurface(const VulkanSurface&) = delete;
    VulkanSurface& operator=(const VulkanSurface&) = delete;

    VulkanSurface(VulkanSurface&& other) noexcept;
    VulkanSurface& operator=(VulkanSurface&& other) noexcept;

    VkSurfaceKHR handle() const noexcept;
    VkInstance instance_handle() const noexcept;

private:
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkInstance m_instance = VK_NULL_HANDLE;
};
