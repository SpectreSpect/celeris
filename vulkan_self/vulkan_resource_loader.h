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

    void upload_vertex_buffer(
        const void* src_data,
        VkDeviceSize size_bytes,
        VulkanBuffer& dst_buffer,
        VkDeviceSize dst_offset = 0
    );

    void submit();

private:
    struct UploadRequest {
        VulkanBuffer* dst_buffer = nullptr;
        VkDeviceSize staging_offset = 0;
        VkDeviceSize dst_offset = 0;
        VkDeviceSize size_bytes = 0;
        VkPipelineStageFlags dst_stage = 0;
        VkAccessFlags dst_access = 0;
    };

private:
    VulkanBuffer m_staging_buffer;
    VulkanQueue* m_queue = nullptr;
    VulkanCommandBuffer m_command_buffer;
    VulkanFence m_fence;
    VkDeviceSize m_loaded_data_size = 0;

    std::vector<UploadRequest> m_upload_requests;
};
