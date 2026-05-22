#pragma once

#include <vector>
#include <cstddef>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"
#include "vulkan_buffer.h"
#include "vulkan_command_buffer.h"
#include "vulkan_fence.h"

class VulkanPhysicalDevice;
class VulkanDevice;
class VulkanQueue;
class VulkanCommandPool;
class VulkanEngine;
class VulkanImage;
class VulkanTexture2D;
class CpuImage;

class VulkanResourceLoader {
public:
    _XCLASS_NAME(VulkanResourceLoader);

    explicit VulkanResourceLoader(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VulkanQueue& upload_queue,
        VulkanCommandPool& upload_command_pool,
        VkDeviceSize size_bytes
    );

    explicit VulkanResourceLoader(VulkanEngine& engine, VkDeviceSize size_bytes);

    VulkanResourceLoader(const VulkanResourceLoader&) = delete;
    VulkanResourceLoader& operator=(const VulkanResourceLoader&) = delete;

    void upload(
        const void* src_data,
        VkDeviceSize size_bytes,
        VulkanBuffer& dst_buffer,
        VkDeviceSize dst_offset,
        VkPipelineStageFlags dst_stage,
        VkAccessFlags dst_access
    );

    void upload(
        const void* src_data,
        VkExtent3D src_data_extent,
        uint32_t src_count_array_layers,
        VulkanImage& dst_image,
        VkImageAspectFlags dst_aspect_mask,
        VkOffset3D dst_image_offset,
        uint32_t dst_mip_level,
        uint32_t dst_base_array_layer,
        VkImageLayout dst_old_layout,
        VkPipelineStageFlags dst_old_stage,
        VkAccessFlags dst_old_access,
        VkImageLayout dst_finish_layout,
        VkPipelineStageFlags dst_finish_stage,
        VkAccessFlags dst_finish_access
    );

    void upload_sampled_image_2d(
        const void* src_data,
        VkExtent2D extent,
        VulkanImage& dst_image,
        VkPipelineStageFlags shader_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VkOffset3D image_offset = {0, 0, 0},
        uint32_t mip_level = 0,
        uint32_t base_array_layer = 0
    );

    void upload_sampled_texture_2d(
        const CpuImage& cpu_image,
        VulkanTexture2D& texture
    );

    void upload_vertex_buffer(
        const void* src_data,
        VkDeviceSize size_bytes,
        VulkanBuffer& dst_buffer,
        VkDeviceSize dst_offset = 0
    );

    void upload_index_buffer(
        const void* src_data,
        VkDeviceSize size_bytes,
        VulkanBuffer& dst_buffer,
        VkDeviceSize dst_offset = 0
    );

    void upload_storage_buffer(
        const void* src_data,
        VkDeviceSize size_bytes,
        VulkanBuffer& dst_buffer,
        VkPipelineStageFlags shader_stage,
        VkAccessFlags shader_access,
        VkDeviceSize dst_offset = 0
    );

    void upload_compute_storage_buffer(
        const void* src_data,
        VkDeviceSize size_bytes,
        VulkanBuffer& dst_buffer,
        VkDeviceSize dst_offset = 0
    );

    void submit();

private:
    struct BufferUploadRequest {
        VulkanBuffer* dst_buffer = nullptr;
        VkDeviceSize staging_offset = 0;
        VkDeviceSize dst_offset = 0;
        VkDeviceSize size_bytes = 0;
        VkPipelineStageFlags dst_stage = 0;
        VkAccessFlags dst_access = 0;
    };

    struct ImageUploadRequest {
        // ===src part===
        VkDeviceSize staging_offset = 0;
        VkExtent3D src_extent = {0, 0, 0};
        uint32_t src_count_array_layers = 1;

        // ===dst part===
        VulkanImage* dst_image = nullptr;
        VkImageAspectFlags dst_aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;

        VkOffset3D dst_image_offset = {0, 0, 0};
        uint32_t dst_mip_level = 0;
        uint32_t dst_base_array_layer = 0;

        VkImageLayout dst_old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkPipelineStageFlags dst_old_stage = 0;
        VkAccessFlags dst_old_access = 0;

        VkImageLayout dst_finish_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkPipelineStageFlags dst_finish_stage = 0;
        VkAccessFlags dst_finish_access = 0;
    };

    struct TextureLayoutUpdate {
        VulkanTexture2D* texture = nullptr;
        VkImageLayout final_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

private:
    VulkanBuffer m_staging_buffer;
    VulkanQueue* m_queue = nullptr;
    VulkanCommandBuffer m_command_buffer;
    VulkanFence m_fence;
    VkDeviceSize m_loaded_data_size = 0;

    std::vector<BufferUploadRequest> m_buffer_upload_requests;
    std::vector<ImageUploadRequest> m_image_upload_requests;
    std::vector<TextureLayoutUpdate> m_texture_upload_requests;
};
