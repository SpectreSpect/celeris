#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../logger/logger_header.h"

#include "vulkan_image.h"
#include "vulkan_image_view.h"
#include "vulkan_sampler.h"

class VulkanPhysicalDevice;
class VulkanDevice;

struct VulkanTexture2DDesc {
    VkExtent2D extent2d = {0, 0};
    VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
    uint32_t mip_levels = 0;

    VkImageUsageFlags usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT;

    VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageCreateFlags image_flags = 0;

    VkImageAspectFlags view_aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
    uint32_t view_base_mip_level = 0;
    uint32_t view_mip_levels_count = 0; // 0 means all mip levels from view_base_mip_level
    uint32_t view_base_array_layer = 0;
    uint32_t view_array_layers_count = 1;
    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;

    VkFilter sampler_mag_filter = VK_FILTER_LINEAR;
    VkFilter sampler_min_filter = VK_FILTER_LINEAR;

    VkSamplerAddressMode sampler_address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode sampler_address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode sampler_address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    VkSamplerMipmapMode sampler_mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VkBool32 sampler_anisotropy_enable = VK_FALSE;
    float sampler_max_anisotropy = 1.0f;

    VkBorderColor sampler_border_color = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    VkBool32 sampler_unnormalized_coordinates = VK_FALSE;

    VkBool32 sampler_compare_enable = VK_FALSE;
    VkCompareOp sampler_compare_op = VK_COMPARE_OP_ALWAYS;

    float sampler_mip_lod_bias = 0.0f;
    float sampler_min_lod = 0.0f;
    float sampler_max_lod = -1.0f; // negative means mip_levels - 1
};

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
        const VulkanTexture2DDesc& desc
    );

    explicit VulkanTexture2D(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkExtent2D extent2d,
        uint32_t mip_levels = 0
    );

    explicit VulkanTexture2D(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkExtent2D extent2d,
        VkFormat format,
        uint32_t mip_levels = 0
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
    uint32_t mip_levels() const noexcept;

    VkImageLayout layout() const noexcept;
    VkImageLayout texture_layout() const noexcept;
    VkDescriptorImageInfo descriptor_image_info() const noexcept;

    void set_layout(VkImageLayout layout) noexcept;

    static VulkanTexture2DDesc deafult_desc(VkExtent2D extent2d);
    static VulkanTexture2DDesc default_desc(VkExtent2D extent2d);

    static uint32_t calculate_mip_levels(VkExtent2D extent2d);

    void transition_layout(
        VulkanCommandBuffer& command_buffer,
        VkImageLayout new_layout,
        VkPipelineStageFlags src_stage,
        VkPipelineStageFlags dst_stage,
        VkAccessFlags src_access,
        VkAccessFlags dst_access,
        uint32_t base_mip_level = 0,
        uint32_t level_count = 0
    );

private:
    VulkanImage m_image;
    VulkanImageView m_view;
    VulkanSampler m_sampler;
    VkImageLayout m_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

private:
    static VulkanImage create_image(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        const VulkanTexture2DDesc& desc
    );

    static VulkanImageView create_view(
        const VulkanDevice& device,
        const VulkanImage& image,
        const VulkanTexture2DDesc& desc
    );

    static VkSamplerCreateInfo create_sampler_desc(
        const VulkanTexture2DDesc& desc,
        uint32_t mip_levels
    );
};