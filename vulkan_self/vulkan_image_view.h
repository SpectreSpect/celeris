#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

#include "logger/logger_header.h"

class VulkanDevice;
class VulkanSwapchain;
class VulkanImageTemp;

class VulkanImageView {
public:
    _XCLASS_NAME(VulkanImageView);

    VulkanImageView(const VkImageViewCreateInfo& desc, const VulkanDevice& device);
    ~VulkanImageView();
    void destroy();

    VulkanImageView(const VulkanImageView&) = delete;
    VulkanImageView& operator=(const VulkanImageView&) = delete;
    
    VulkanImageView(VulkanImageView&& other) noexcept;
    VulkanImageView& operator=(VulkanImageView&& other) noexcept;

    VkImageView handle() const noexcept;

    static VkImageViewCreateInfo create_swapchain_desc(VkImage image, VkFormat format);
    static std::vector<VulkanImageView> from_swapchain(const VulkanDevice& device, const VulkanSwapchain& swapchain);

    static VkImageViewCreateInfo create_depth_desc(VkImage image, VkFormat format);
    static std::vector<VulkanImageView> from_depth_images(const VulkanDevice& device, 
                                                          const std::vector<VulkanImageTemp>& images, 
                                                          VkFormat format);
private:
    VkImageView m_image_view = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
};
