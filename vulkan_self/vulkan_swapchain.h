#pragma once

#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"

class VulkanPhysicalDevice;
class VulkanDevice;
class VulkanSurface;
class Window;

class VulkanSwapchain {
public:
    _XCLASS_NAME(VulkanSwapchain);

    explicit VulkanSwapchain(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        const VulkanSurface& surface,
        const Window& window
    );
    ~VulkanSwapchain();
    void destroy();

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    VulkanSwapchain(VulkanSwapchain&& other) noexcept;
    VulkanSwapchain& operator=(VulkanSwapchain&& other) noexcept;

    const VkSwapchainKHR& handle() const noexcept;
    VkImage image(uint32_t index = 0) const;

    const std::vector<VkImage>& images() const noexcept;

    VkFormat image_format() const noexcept;
    VkExtent2D extent() const noexcept;

private:
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_images;

    VkFormat m_image_format = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent{};

    VkDevice m_device = VK_NULL_HANDLE;

private:
    VkSurfaceFormatKHR choose_swap_surface_format(
        const std::vector<VkSurfaceFormatKHR>& available_formats
    ) const;

    VkPresentModeKHR choose_swap_present_mode(
        const std::vector<VkPresentModeKHR>& available_present_modes
    ) const;

    VkExtent2D choose_swap_extent(
        const VkSurfaceCapabilitiesKHR& capabilities,
        const Window& window
    ) const;
};
