#pragma once

#include <optional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../logger/logger_header.h"
#include "../vulkan_memory.h"

class VulkanPhysicalDevice;
class VulkanDevice;
class VulkanCommandBuffer;
class VulkanBuffer;

class VulkanImage {
public:
    _XCLASS_NAME(VulkanImage);

    explicit VulkanImage(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkExtent3D extent,
        VkFormat format,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags memory_properties,
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
        VkImageType image_type = VK_IMAGE_TYPE_2D,
        uint32_t mip_levels = 1,
        uint32_t array_layers = 1,
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
        VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED
    );

    explicit VulkanImage(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkExtent2D extent,
        VkFormat format,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags memory_properties,
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL
    );

    ~VulkanImage() noexcept;

private:
    void destroy() noexcept;

public:
    VulkanImage(const VulkanImage&) = delete;
    VulkanImage& operator=(const VulkanImage&) = delete;

    VulkanImage(VulkanImage&& other) noexcept;
    VulkanImage& operator=(VulkanImage&& other) noexcept;

    VkImage handle() const noexcept;
    VkExtent3D extent() const noexcept;
    VkExtent2D extent2d() const noexcept;
    VkFormat format() const noexcept;
    VkImageUsageFlags usage() const noexcept;
    uint32_t mip_levels() const noexcept;
    uint32_t array_layers() const noexcept;

    bool has_usage(VkImageUsageFlags usage) const noexcept;

    void memory_barrier(
        VulkanCommandBuffer& command_buffer,
        VkPipelineStageFlags src_stage,
        VkAccessFlags src_access,
        VkPipelineStageFlags dst_stage,
        VkAccessFlags dst_access,
        VkImageLayout old_layout,
        VkImageLayout new_layout,
        VkImageAspectFlags aspect_mask,
        uint32_t base_mip_level = 0,
        uint32_t level_count = 1,
        uint32_t base_array_layer = 0,
        uint32_t layer_count = 1
    ) const;

    void barrier_undefined_to_transfer_dst(
        VulkanCommandBuffer& command_buffer,
        VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT
    ) const;

    void barrier_transfer_write_to_shader_read(
        VulkanCommandBuffer& command_buffer,
        VkPipelineStageFlags shader_stage,
        VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT
    ) const;

    void copy_from_buffer(
        VulkanCommandBuffer& command_buffer,
        const VulkanBuffer& src_buffer,
        VkDeviceSize buffer_offset = 0,
        VkOffset3D image_offset = {0, 0, 0},
        VkExtent3D image_extent = {0, 0, 0},
        uint32_t mip_level = 0,
        uint32_t base_array_layer = 0,
        uint32_t layer_count = 1,
        VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT
    ) const;
    
private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkImage m_image = VK_NULL_HANDLE;

    VkExtent3D m_extent{};
    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags m_usage = 0;
    VkImageTiling m_tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageType m_image_type = VK_IMAGE_TYPE_2D;
    uint32_t m_mip_levels = 1;
    uint32_t m_array_layers = 1;
    VkSampleCountFlagBits m_samples = VK_SAMPLE_COUNT_1_BIT;

    std::optional<VulkanMemory> m_memory = std::nullopt;
};
