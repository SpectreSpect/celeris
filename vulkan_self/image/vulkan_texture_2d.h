#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../logger/logger_header.h"

#include "vulkan_image.h"
#include "vulkan_image_view.h"
#include "vulkan_sampler.h"

class VulkanPhysicalDevice;
class VulkanDevice;

class VulkanTexture2D {
public:
    _XCLASS_NAME(VulkanTexture2D);    

    // Так как копировать эти объекты нельзя, поэтому &&
    explicit VulkanTexture2D(
        VulkanImage&& image,
        VulkanImageView&& view,
        VulkanSampler&& sampler,
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    explicit VulkanTexture2D(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkExtent2D extent2d
    );

    ~VulkanTexture2D() noexcept = default;

    VulkanTexture2D(const VulkanTexture2D&) = delete;
    VulkanTexture2D& operator=(const VulkanTexture2D&) = delete;

    VulkanTexture2D(VulkanTexture2D&&) noexcept = default;
    VulkanTexture2D& operator=(VulkanTexture2D&&) noexcept = default;

    VulkanImage& image() noexcept;
    const VulkanImage& image() const noexcept;
    VulkanImageView& view() noexcept;
    const VulkanImageView& view() const noexcept;
    VulkanSampler& sampler() noexcept;
    const VulkanSampler& sampler() const noexcept;
    VkExtent3D extent() const noexcept;
    VkExtent2D extent2d() const noexcept;
    VkFormat format() const noexcept;
    VkImageLayout layout() const noexcept;
    VkImageLayout texture_layout() const noexcept;
    VkDescriptorImageInfo descriptor_image_info() const noexcept;

    void set_layout(VkImageLayout layout) noexcept;

private:
    VulkanImage m_image;
    VulkanImageView m_view;
    VulkanSampler m_sampler;
    VkImageLayout m_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
};
