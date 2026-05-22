#pragma once

#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../logger/logger_header.h"

class VulkanDevice;
class VulkanSwapchain;
class VulkanImage;

class VulkanImageView {
public:
    _XCLASS_NAME(VulkanImageView);

    VulkanImageView(const VkImageViewCreateInfo& desc, const VulkanDevice& device);
    VulkanImageView(
        const VulkanDevice& device,
        const VulkanImage& image,
        VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT,
        uint32_t base_mip_level = 0,
        uint32_t mip_levels_count = 1,
        uint32_t base_array_level = 0,
        uint32_t array_levels_count = 1
    );
    ~VulkanImageView() noexcept;

    VulkanImageView(const VulkanImageView&) = delete;
    VulkanImageView& operator=(const VulkanImageView&) = delete;
    
    VulkanImageView(VulkanImageView&& other) noexcept;
    VulkanImageView& operator=(VulkanImageView&& other) noexcept;

    VkImageView handle() const noexcept;

    static VkImageViewCreateInfo create_swapchain_desc(VkImage image, VkFormat format);
    static std::vector<VulkanImageView> from_swapchain(const VulkanDevice& device, const VulkanSwapchain& swapchain);

private:
    VkImageView m_image_view = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

private:
    void destroy() noexcept;
    void create_view(const VkImageViewCreateInfo& desc, const VulkanDevice& device);
};
