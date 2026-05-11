#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"

class VulkanDevice;
class VulkanSwapchain;
class VulkanCommandBuffer;
class VulkanFramebuffer;

class VulkanRenderPass {
public:
    _XCLASS_NAME(VulkanRenderPass);

    explicit VulkanRenderPass(const VulkanDevice& device, const VkRenderPassCreateInfo& desc);
    explicit VulkanRenderPass(const VulkanDevice& device, const VulkanSwapchain& swapchain);

    ~VulkanRenderPass();
    void destroy();

    VulkanRenderPass(const VulkanRenderPass&) = delete;
    VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;

    VulkanRenderPass(VulkanRenderPass&& other) noexcept;
    VulkanRenderPass& operator=(VulkanRenderPass&& other) noexcept;

    VkRenderPass handle() const noexcept;

    void begin(
        VulkanCommandBuffer& command_buffer,
        const VulkanFramebuffer& framebuffer,
        VkExtent2D extent,
        VkOffset2D offset = {0, 0},
        VkClearValue clear_color = {{0.0f, 0.0f, 0.0f, 1.0f}}
    );

    void begin(
        VulkanCommandBuffer& command_buffer,
        const VulkanFramebuffer& framebuffer,
        const VulkanSwapchain& swapchain,
        VkClearValue clear_color = {{0.0f, 0.0f, 0.0f, 1.0f}}
    );

    void end(VulkanCommandBuffer& command_buffer);

private:
    VkRenderPass m_render_pass = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

private:
    void create(const VkRenderPassCreateInfo& desc);
};
