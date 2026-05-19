#include "vulkan_resource_loader.h"

#include <string>

#include "vulkan_device.h"
#include "vulkan_queue.h"
#include "vulkan_engine.h"

VulkanResourceLoader::VulkanResourceLoader(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VulkanQueue& upload_queue,
    VulkanCommandPool& upload_command_pool,
    VkDeviceSize size_bytes)
    :   m_staging_buffer(VulkanBuffer::create_staging_buffer(physical_device, device, size_bytes)),
        m_queue(&upload_queue),
        m_command_buffer(device, upload_command_pool),
        m_fence(device)
{
    LOG_METHOD();

    logger.check(m_queue->handle() != VK_NULL_HANDLE, "Queue is not initialized");
}

VulkanResourceLoader::VulkanResourceLoader(VulkanEngine& engine, VkDeviceSize size_bytes)
    :   VulkanResourceLoader(
            engine.physical_device(),
            engine.device(),
            engine.device().graphics_queue(),
            engine.upload_command_pool(),
            size_bytes) {}

void VulkanResourceLoader::upload(
    const void* src_data,
    VkDeviceSize size_bytes,
    VulkanBuffer& dst_buffer,
    VkDeviceSize dst_offset,
    VkPipelineStageFlags dst_stage,
    VkAccessFlags dst_access)
{
    LOG_METHOD();

    logger.check(src_data != nullptr, "Pointer to the source data is null");
    logger.check(size_bytes != 0, "Attempt to load data of zero size");

    logger.check(size_bytes <= m_staging_buffer.size())
        << "There is not enough space in the resource loader to load this much data "
        << "(" << clr("size_bytes", LoggerPalette::blue) << " = " 
        << clr(std::to_string(size_bytes), LoggerPalette::orange) 
        << ", while the maximum size is "
        << clr(std::to_string(m_staging_buffer.size()), LoggerPalette::orange)
        << ")\n";

    logger.check(dst_stage != 0, "Destination pipeline stage must not be zero");
    logger.check(dst_access != 0, "Destination access mask must not be zero");

    logger.check(dst_buffer.handle() != VK_NULL_HANDLE, "Destination buffer is not initialized");

    logger.check(
        dst_buffer.has_usage(VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        "Destination buffer must be created with VK_BUFFER_USAGE_TRANSFER_DST_BIT"
    );

    logger.check(m_staging_buffer.handle() != VK_NULL_HANDLE, "Staging buffer is not initialized");

    logger.check(dst_offset <= dst_buffer.size(), "Destination offset is out of bounds");
    logger.check(size_bytes <= dst_buffer.size() - dst_offset, "Destination upload range is out of bounds");

    if (size_bytes > m_staging_buffer.size() - m_loaded_data_size) {
        submit();
    }

    VkDeviceSize staging_offset = m_loaded_data_size;

    m_staging_buffer.upload(
        src_data,
        size_bytes,
        staging_offset
    );

    m_upload_requests.push_back(UploadRequest{
        .dst_buffer = &dst_buffer,
        .staging_offset = staging_offset,
        .dst_offset = dst_offset,
        .size_bytes = size_bytes,
        .dst_stage = dst_stage,
        .dst_access = dst_access
    });

    m_loaded_data_size += size_bytes;
}

void VulkanResourceLoader::upload_vertex_buffer(
    const void* src_data,
    VkDeviceSize size_bytes,
    VulkanBuffer& dst_buffer,
    VkDeviceSize dst_offset)
{
    LOG_METHOD();

    logger.check(dst_buffer.has_usage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT), "Destinition buffer must be a vertex buffer");

    upload(
        src_data, 
        size_bytes, 
        dst_buffer, 
        dst_offset,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
    );
}

void VulkanResourceLoader::submit() {
    LOG_METHOD();

    if (m_upload_requests.empty()) {
        return;
    }

    logger.check(m_staging_buffer.handle() != VK_NULL_HANDLE, "Staging buffer is not initialized");
    logger.check(m_command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initizlied");
    logger.check(m_queue != nullptr, "Queue pointer specify to null");
    logger.check(m_queue->handle() != VK_NULL_HANDLE, "Queue is not initialized");
    logger.check(m_fence.handle() != VK_NULL_HANDLE, "Fence is not initialized");

    m_fence.reset();
    m_command_buffer.reset();
    
    {
        auto upload_scope = m_command_buffer.begin_scope();
        
        for (const UploadRequest& request : m_upload_requests) {
            logger.check(request.dst_buffer != nullptr, "Destination buffer pointer is null");
            logger.check(request.dst_buffer->handle() != VK_NULL_HANDLE, "Destination buffer is not initialized or was destroyed");
            
            m_staging_buffer.copy_to(
                m_command_buffer,
                *request.dst_buffer,
                request.size_bytes,
                request.staging_offset,
                request.dst_offset
            );

            request.dst_buffer->memory_barrier(
                m_command_buffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                request.dst_stage,
                request.dst_access,
                request.dst_offset,
                request.size_bytes
            );
        }
    }

    m_queue->submit(nullptr, 0, m_command_buffer, nullptr, &m_fence);
    m_fence.wait();

    m_upload_requests.clear();
    m_loaded_data_size = 0;
}
