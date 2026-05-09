#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"

class VulkanDevice;
class VulkanSwapchain;

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

private:
    VkRenderPass m_render_pass = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

private:
    void create(const VkRenderPassCreateInfo& desc);
};
