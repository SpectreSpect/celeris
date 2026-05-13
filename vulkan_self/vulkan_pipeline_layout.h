#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"

class VulkanDevice;

class VulkanPipelineLayout {
public:
    _XCLASS_NAME(VulkanPipelineLayout);

    explicit VulkanPipelineLayout(const VulkanDevice& device);
    ~VulkanPipelineLayout() noexcept;
    void destroy() noexcept;

    VulkanPipelineLayout(const VulkanPipelineLayout&) = delete;
    VulkanPipelineLayout& operator=(const VulkanPipelineLayout&) = delete;
    
    VulkanPipelineLayout(VulkanPipelineLayout&& other) noexcept;
    VulkanPipelineLayout& operator=(VulkanPipelineLayout&& other) noexcept;

    VkPipelineLayout handle() const noexcept;

private:
    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
};
