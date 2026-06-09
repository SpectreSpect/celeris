#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../logger/logger_header.h"

class VulkanDevice;

class VulkanSampler {
public:
    _XCLASS_NAME(VulkanSampler);

    explicit VulkanSampler(
        const VulkanDevice& device,
        const VkSamplerCreateInfo& desc
    );

    explicit VulkanSampler(
        const VulkanDevice& device,
        VkFilter filter = VK_FILTER_LINEAR,
        VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR
    );
    ~VulkanSampler() noexcept;

    VulkanSampler(const VulkanSampler&) = delete;
    VulkanSampler& operator=(const VulkanSampler&) = delete;

    VulkanSampler(VulkanSampler&& other) noexcept;
    VulkanSampler& operator=(VulkanSampler&& other) noexcept;

    VkSampler handle() const noexcept;

private:
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

private:
    void destroy() noexcept;
    void create_sampler(
        const VulkanDevice& device,
        const VkSamplerCreateInfo& desc
    );
};
