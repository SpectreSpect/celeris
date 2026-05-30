#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../logger/logger_header.h"

#include "vulkan_image.h"
#include "vulkan_image_view.h"
#include "vulkan_sampler.h"

class VulkanPhysicalDevice;
class VulkanDevice;

class Cubemap {
public:
    _XCLASS_NAME(Cubemap);

    static constexpr uint32_t face_count = 6;

    // Так как копировать эти объекты нельзя, поэтому &&
    explicit Cubemap(
        VulkanImage&& image,
        VulkanImageView&& view,
        VulkanSampler&& sampler,
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    explicit Cubemap(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkExtent2D extent2d,
        uint32_t mip_levels = 0
    );

    explicit Cubemap(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkExtent2D extent2d,
        VkFormat format,
        uint32_t mip_levels = 0
    );

    ~Cubemap() noexcept = default;

    Cubemap(const Cubemap&) = delete;
    Cubemap& operator=(const Cubemap&) = delete;

    Cubemap(Cubemap&&) noexcept = default;
    Cubemap& operator=(Cubemap&&) noexcept = default;

    VulkanImage& image() noexcept;
    const VulkanImage& image() const noexcept;
    VulkanImageView& view() noexcept;
    const VulkanImageView& view() const noexcept;
    VulkanSampler& sampler() noexcept;
    const VulkanSampler& sampler() const noexcept;

    VkExtent3D extent() const noexcept;
    VkExtent2D extent2d() const noexcept;
    VkFormat format() const noexcept;
    uint32_t mip_levels() const noexcept;
    uint32_t array_layers() const noexcept;

    VkImageLayout layout() const noexcept;
    VkImageLayout texture_layout() const noexcept;
    VkDescriptorImageInfo descriptor_image_info() const noexcept;

    void set_layout(VkImageLayout layout) noexcept;

    static uint32_t calculate_mip_levels(VkExtent2D extent2d);

    void transition_layout(
        VulkanCommandBuffer& command_buffer,
        VkImageLayout new_layout,
        VkPipelineStageFlags src_stage,
        VkPipelineStageFlags dst_stage,
        VkAccessFlags src_access,
        VkAccessFlags dst_access,
        uint32_t base_mip_level = 0,
        uint32_t level_count = 0,
        uint32_t base_array_layer = 0,
        uint32_t layer_count = face_count
    );

    void generate_mipmaps(VulkanCommandBuffer& command_buffer);

private:
    VulkanImage m_image;
    VulkanImageView m_view;
    VulkanSampler m_sampler;
    VkImageLayout m_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

private:
    static VulkanImage create_image(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkExtent2D extent2d,
        VkFormat format,
        uint32_t mip_levels
    );

    static VkSamplerCreateInfo create_sampler_desc(
        uint32_t mip_levels,
        VkFilter filter = VK_FILTER_LINEAR,
        VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR
    );
};