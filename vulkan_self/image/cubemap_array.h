#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>

#include "../logger/logger_header.h"

#include "vulkan_image.h"
#include "vulkan_image_view.h"
#include "vulkan_sampler.h"

class VulkanPhysicalDevice;
class VulkanDevice;
class VulkanCommandBuffer;

class CubemapArray {
public:
    _XCLASS_NAME(CubemapArray);

    static constexpr uint32_t face_count = 6;

    enum class StorageImageUsage : uint8_t {
        Disabled,
        Enabled
    };

    // CPU layer order for uploads/copies/storage writes:
    // layer = cubemap_id * 6 + face_id
    // face_id usually follows Vulkan cubemap order:
    // +X, -X, +Y, -Y, +Z, -Z.
    static constexpr uint32_t calculate_layer_index(
        uint32_t cubemap_id,
        uint32_t face_id
    ) noexcept {
        return cubemap_id * face_count + face_id;
    }

    explicit CubemapArray(
        VulkanImage&& image,
        VulkanImageView&& view,
        VulkanSampler&& sampler,
        uint32_t cubemap_count,
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    // Default sampled cubemap array. Good for texture upload via transfer/copy.
    // Uses VK_FORMAT_R8G8B8A8_SRGB and does NOT request VK_IMAGE_USAGE_STORAGE_BIT.
    explicit CubemapArray(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkExtent2D extent2d,
        uint32_t cubemap_count,
        uint32_t mip_levels = 0
    );

    // Sampled cubemap array. Good for texture upload via transfer/copy.
    // Does NOT request VK_IMAGE_USAGE_STORAGE_BIT.
    explicit CubemapArray(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkExtent2D extent2d,
        VkFormat format,
        uint32_t cubemap_count,
        uint32_t mip_levels = 0
    );

    // Use this only if a compute shader will write into this cubemap array
    // as a storage image. The selected format must support STORAGE_IMAGE.
    // For example, VK_FORMAT_R8G8B8A8_SRGB often does NOT support storage usage;
    // use VK_FORMAT_R8G8B8A8_UNORM or VK_FORMAT_R16G16B16A16_SFLOAT instead.
    explicit CubemapArray(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkExtent2D extent2d,
        VkFormat format,
        uint32_t cubemap_count,
        StorageImageUsage storage_image_usage,
        uint32_t mip_levels = 0
    );

    ~CubemapArray() noexcept = default;

    CubemapArray(const CubemapArray&) = delete;
    CubemapArray& operator=(const CubemapArray&) = delete;

    CubemapArray(CubemapArray&&) noexcept = default;
    CubemapArray& operator=(CubemapArray&&) noexcept = default;

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
    uint32_t cubemap_count() const noexcept;

    uint32_t base_layer(uint32_t cubemap_id) const;
    uint32_t layer_index(uint32_t cubemap_id, uint32_t face_id) const;

    VkImageLayout layout() const noexcept;
    VkImageLayout texture_layout() const noexcept;
    VkDescriptorImageInfo descriptor_image_info() const noexcept;

    void set_layout(VkImageLayout layout) noexcept;

    static uint32_t calculate_mip_levels(VkExtent2D extent2d);
    static uint32_t calculate_array_layers(uint32_t cubemap_count);

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
        uint32_t layer_count = 0
    );

    // Default assumes mip 0 was filled by transfer/copy into TRANSFER_DST_OPTIMAL.
    // If mip 0 was filled by compute shader as a storage image, call:
    // generate_mipmaps(cmd, VK_IMAGE_LAYOUT_GENERAL,
    //                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //                  VK_ACCESS_SHADER_WRITE_BIT);
    void generate_mipmaps(
        VulkanCommandBuffer& command_buffer,
        VkImageLayout base_mip_old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VkPipelineStageFlags base_mip_old_stage = VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkAccessFlags base_mip_old_access = VK_ACCESS_TRANSFER_WRITE_BIT
    );

private:
    VulkanImage m_image;
    VulkanImageView m_view;
    VulkanSampler m_sampler;
    VkImageLayout m_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    uint32_t m_cubemap_count = 0;

private:
    static VulkanImage create_image(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkExtent2D extent2d,
        VkFormat format,
        uint32_t cubemap_count,
        StorageImageUsage storage_image_usage,
        uint32_t mip_levels
    );

    static VkSamplerCreateInfo create_sampler_desc(
        uint32_t mip_levels,
        VkFilter filter = VK_FILTER_LINEAR,
        VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR
    );
};
