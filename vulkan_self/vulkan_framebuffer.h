#pragma once

#include <vector>
#include <span>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"

class VulkanDevice;
class VulkanRenderPass;
class VulkanImageView;

class VulkanFramebuffer {
public:
    _XCLASS_NAME(VulkanFramebuffer);

    explicit VulkanFramebuffer(const VulkanDevice& device, const VkFramebufferCreateInfo& desc);
    explicit VulkanFramebuffer(
        const VulkanDevice& device,
        const VulkanRenderPass& render_pass,
        std::span<const VulkanImageView> attachments,
        VkExtent2D extent2d,
        uint32_t layers = 1
    );
    ~VulkanFramebuffer();
    void destroy();

    VulkanFramebuffer(const VulkanFramebuffer&) = delete;
    VulkanFramebuffer& operator=(const VulkanFramebuffer&) = delete;

    VulkanFramebuffer(VulkanFramebuffer&& other) noexcept;
    VulkanFramebuffer& operator=(VulkanFramebuffer&& other) noexcept;

    VkFramebuffer handle() const noexcept;

    static std::vector<VulkanFramebuffer> from_image_views(
        std::span<const VulkanImageView> image_views,
        const VulkanDevice& device,
        const VulkanRenderPass& render_pass,
        VkExtent2D extent2d,
        uint32_t layers = 1
    );

    static std::vector<VulkanFramebuffer> from_image_views(
        const std::vector<VulkanImageView>& color_image_views,
        const std::vector<VulkanImageView>& depth_image_views,
        const VulkanDevice& device,
        const VulkanRenderPass& render_pass,
        VkExtent2D extent
    );

private:
    VkFramebuffer m_framebuffer = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    void create(const VkFramebufferCreateInfo& desc);
};