#include "vulkan_resource_loader.h"

#include <string>

#include "vulkan_device.h"
#include "vulkan_queue.h"
#include "vulkan_engine.h"
#include "image/cpu_image.h"
#include "image/vulkan_image.h"
#include "image/vulkan_texture_2d.h"
#include "utils.h"

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

    m_buffer_upload_requests.push_back(BufferUploadRequest{
        .dst_buffer = &dst_buffer,
        .staging_offset = staging_offset,
        .dst_offset = dst_offset,
        .size_bytes = size_bytes,
        .dst_stage = dst_stage,
        .dst_access = dst_access
    });

    m_loaded_data_size += size_bytes;
}

void VulkanResourceLoader::upload(
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
    VkAccessFlags dst_finish_access)
{
    LOG_METHOD();

    logger.check(src_data != nullptr, "Pointer to the source image data is null");
    logger.check(src_data_extent.width != 0, "Source image width is zero");
    logger.check(src_data_extent.height != 0, "Source image height is zero");
    logger.check(src_data_extent.depth != 0, "Source image depth is zero");
    logger.check(src_count_array_layers != 0, "Source array layer count is zero");

    VkDeviceSize src_size_bytes = Utils::image_size_bytes(
        src_data_extent, 
        dst_image.format(), 
        1, 
        src_count_array_layers
    );

    logger.check(src_size_bytes <= m_staging_buffer.size())
        << "There is not enough space in the resource loader to upload this image "
        << "(" << clr("src_size_bytes", LoggerPalette::blue) << " = "
        << clr(std::to_string(src_size_bytes), LoggerPalette::orange)
        << ", while the maximum size is "
        << clr(std::to_string(m_staging_buffer.size()), LoggerPalette::orange)
        << ")\n";
    
    logger.check(dst_mip_level < dst_image.mip_levels(), "Destination mip level is out of range");
    logger.check(dst_base_array_layer < dst_image.array_layers(), "Destination base array layer is out of range");
    logger.check(src_count_array_layers <= dst_image.array_layers() - dst_base_array_layer, "Destination array layer range is out of bounds");

    logger.check(dst_old_stage != 0, "Destination old pipeline stage must not be zero");
    logger.check(dst_finish_stage != 0, "Destination finish pipeline stage must not be zero");
    logger.check(dst_finish_access != 0, "Destination finish access mask must not be zero");
    logger.check(dst_finish_layout != VK_IMAGE_LAYOUT_UNDEFINED, "Destination finish image layout must not be UNDEFINED");
    logger.check(dst_aspect_mask != 0, "Destination image aspect mask must not be zero");

    logger.check(dst_image.handle() != VK_NULL_HANDLE, "Destination image is not initialized");

    logger.check(
        dst_image.has_usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT),
        "Destination image must be created with VK_IMAGE_USAGE_TRANSFER_DST_BIT"
    );

    if (src_size_bytes > m_staging_buffer.size() - m_loaded_data_size) {
        submit();
    }

    VkDeviceSize staging_offset = m_loaded_data_size;

    m_staging_buffer.upload(
        src_data,
        src_size_bytes,
        staging_offset
    );

    m_image_upload_requests.push_back(ImageUploadRequest{
        // ===src part===
        .staging_offset = staging_offset,
        .src_extent = src_data_extent,
        .src_count_array_layers = src_count_array_layers,

        // ===dst part===
        .dst_image = &dst_image,
        .dst_aspect_mask = dst_aspect_mask,

        .dst_image_offset = dst_image_offset,
        .dst_mip_level = dst_mip_level,
        .dst_base_array_layer = dst_base_array_layer,

        .dst_old_layout = dst_old_layout,
        .dst_old_stage = dst_old_stage,
        .dst_old_access = dst_old_access,

        .dst_finish_layout = dst_finish_layout,
        .dst_finish_stage = dst_finish_stage,
        .dst_finish_access = dst_finish_access
    });

    m_loaded_data_size += src_size_bytes;
}

void VulkanResourceLoader::upload_sampled_image_2d(
    const void* src_data,
    VkExtent2D extent,
    VulkanImage& dst_image,
    VkPipelineStageFlags shader_stage,
    VkOffset3D image_offset,
    uint32_t mip_level,
    uint32_t base_array_layer)
{
    upload(
        src_data,
        VkExtent3D{extent.width, extent.height, 1},
        1,
        dst_image,
        VK_IMAGE_ASPECT_COLOR_BIT,
        image_offset,
        mip_level,
        base_array_layer,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        0,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        shader_stage,
        VK_ACCESS_SHADER_READ_BIT
    );
}

void VulkanResourceLoader::upload_sampled_texture_2d(
    const CpuImage& cpu_image,
    VulkanTexture2D& texture)
{
    LOG_METHOD();

    logger.check(texture.image().handle() != VK_NULL_HANDLE, "Texture image is not initialized");

    logger.check(cpu_image.format() == texture.format())
        << "CpuImage format does not match texture format\n";
    
    logger.check(
        cpu_image.extent().width == texture.extent().width &&
        cpu_image.extent().height == texture.extent().height &&
        cpu_image.extent().depth == texture.extent().depth
    ) << "CpuImage extent does not match texture extent\n";

    VkExtent3D cpu_image_extent = cpu_image.extent();
    logger.check(cpu_image_extent.width != 0 && cpu_image_extent.height != 0 && cpu_image_extent.depth != 0)
        << "Incorrect" << clr("cpu_image", LoggerPalette::orange) << "extent "
        << "(" << clr(std::to_string(cpu_image_extent.width), LoggerPalette::blue) << ", "
        << clr(std::to_string(cpu_image_extent.height), LoggerPalette::blue) << ", "
        << clr(std::to_string(cpu_image_extent.depth), LoggerPalette::blue) << ")\n";

    logger.check(cpu_image.format() != VK_FORMAT_UNDEFINED)
        << clr("Cpu image", LoggerPalette::orange) << "format is undefined\n";

    upload(
        cpu_image.image_data().data(),
        cpu_image.extent(),
        1,
        texture.image(),

        VK_IMAGE_ASPECT_COLOR_BIT,
        VkOffset3D{0, 0, 0},
        0,
        0,

        texture.layout(),
        texture.layout() == VK_IMAGE_LAYOUT_UNDEFINED
            ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
            : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        texture.layout() == VK_IMAGE_LAYOUT_UNDEFINED
            ? 0
            : VK_ACCESS_SHADER_READ_BIT,

        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT
    );

    m_texture_upload_requests.push_back(TextureLayoutUpdate{
        .texture = &texture,
        .final_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    });
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

void VulkanResourceLoader::upload_index_buffer(
    const void* src_data,
    VkDeviceSize size_bytes,
    VulkanBuffer& dst_buffer,
    VkDeviceSize dst_offset)
{
    LOG_METHOD();

    logger.check(dst_buffer.has_usage(VK_BUFFER_USAGE_INDEX_BUFFER_BIT), "Destinition buffer must be a index buffer");

    upload(
        src_data, 
        size_bytes, 
        dst_buffer, 
        dst_offset,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VK_ACCESS_INDEX_READ_BIT
    );
}

void VulkanResourceLoader::upload_storage_buffer(
    const void* src_data,
    VkDeviceSize size_bytes,
    VulkanBuffer& dst_buffer,
    VkPipelineStageFlags shader_stage,
    VkAccessFlags shader_access,
    VkDeviceSize dst_offset)
{
    LOG_METHOD();

    logger.check(dst_buffer.has_usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT), "Destination buffer must be a storage buffer");

    upload(
        src_data, 
        size_bytes, 
        dst_buffer, 
        dst_offset,
        shader_stage,
        shader_access
    );
}

void VulkanResourceLoader::upload_compute_storage_buffer(
    const void* src_data,
    VkDeviceSize size_bytes,
    VulkanBuffer& dst_buffer,
    VkDeviceSize dst_offset)
{
    upload_storage_buffer(
        src_data, 
        size_bytes, 
        dst_buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        dst_offset
    );
}

void VulkanResourceLoader::submit() {
    LOG_METHOD();

    if (
        m_buffer_upload_requests.empty() && 
        m_image_upload_requests.empty() &&
        m_texture_upload_requests.empty()) {
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
        
        for (const BufferUploadRequest& request : m_buffer_upload_requests) {
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

        for (const ImageUploadRequest& request : m_image_upload_requests) {
            logger.check(request.dst_image != nullptr, "Destination image pointer is null");
            logger.check(request.dst_image->handle() != VK_NULL_HANDLE, "Destination image is not initialized or was destroyed");

            request.dst_image->memory_barrier(
                m_command_buffer,
                request.dst_old_stage,
                request.dst_old_access,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                request.dst_old_layout,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                request.dst_aspect_mask,
                request.dst_mip_level,
                1,
                request.dst_base_array_layer,
                request.src_count_array_layers
            );

            request.dst_image->copy_from_buffer(
                m_command_buffer,
                m_staging_buffer,
                request.staging_offset,
                request.dst_image_offset,
                request.src_extent,
                request.dst_mip_level,
                request.dst_base_array_layer,
                request.src_count_array_layers,
                request.dst_aspect_mask
            );

            request.dst_image->memory_barrier(
                m_command_buffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                request.dst_finish_stage,
                request.dst_finish_access,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                request.dst_finish_layout,
                request.dst_aspect_mask,
                request.dst_mip_level,
                1,
                request.dst_base_array_layer,
                request.src_count_array_layers
            );
        }
    }

    m_queue->submit(nullptr, 0, m_command_buffer, nullptr, &m_fence);
    m_fence.wait();

    for (const TextureLayoutUpdate& request : m_texture_upload_requests) {
        logger.check(request.texture != nullptr, "Pointer to texture specify to null");
        request.texture->set_layout(request.final_layout);
    }

    m_buffer_upload_requests.clear();
    m_image_upload_requests.clear();
    m_texture_upload_requests.clear();
    m_loaded_data_size = 0;
}
