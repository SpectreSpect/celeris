#include "vulkan_image.h"

#include <utility>

#include "vulkan_device.h"
#include "vulkan_physical_device.h"
#include "vulkan_command_buffer.h"
#include "vulkan_buffer.h"

VulkanImage::VulkanImage(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkExtent3D extent,
    VkFormat format,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags memory_properties,
    VkImageTiling tiling,
    VkImageType image_type,
    uint32_t mip_levels,
    uint32_t array_layers,
    VkSampleCountFlagBits samples,
    VkImageLayout initial_layout) 
        :   m_device(device.handle()),
            m_extent(extent),
            m_format(format),
            m_usage(usage),
            m_tiling(tiling),
            m_image_type(image_type),
            m_mip_levels(mip_levels),
            m_array_layers(array_layers),
            m_samples(samples)
{
    LOG_METHOD();

    logger.check(physical_device.handle() != VK_NULL_HANDLE, "Physical device is not initialized");
    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");

    logger.check(extent.width != 0, "Image width is zero");
    logger.check(extent.height != 0, "Image height is zero");
    logger.check(extent.depth != 0, "Image depth is zero");

    logger.check(format != VK_FORMAT_UNDEFINED, "Image format is undefined");
    logger.check(usage != 0, "Image usage flags are empty");
    logger.check(mip_levels != 0, "Image mip levels count is zero");
    logger.check(array_layers != 0, "Image array layers count is zero");

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.flags = 0;
    image_info.imageType = image_type;
    image_info.format = format;
    image_info.extent = extent;
    image_info.mipLevels = mip_levels;
    image_info.arrayLayers = array_layers;
    image_info.samples = samples;
    image_info.tiling = tiling;
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.queueFamilyIndexCount = 0;
    image_info.pQueueFamilyIndices = nullptr;
    image_info.initialLayout = initial_layout;

    VkResult result = vkCreateImage(
        m_device,
        &image_info,
        nullptr,
        &m_image
    );

    logger.check(result == VK_SUCCESS, "Failed to create Vulkan image");

    VkMemoryRequirements memory_requirements{};
    vkGetImageMemoryRequirements(
        m_device,
        m_image,
        &memory_requirements
    );

    m_memory.emplace(
        physical_device,
        device,
        memory_requirements.memoryTypeBits,
        memory_properties,
        memory_requirements.size
    );

    m_memory->bind_to_image(m_image);
}

VulkanImage::VulkanImage(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkExtent2D extent,
    VkFormat format,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags memory_properties,
    VkImageTiling tiling)
        :   VulkanImage (
                physical_device,
                device,
                VkExtent3D{extent.width, extent.height, 1},
                format,
                usage,
                memory_properties,
                tiling,
                VK_IMAGE_TYPE_2D,
                1,
                1,
                VK_SAMPLE_COUNT_1_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED
            ) {}

VulkanImage::~VulkanImage() noexcept {
    destroy();
}

void VulkanImage::destroy() noexcept {
    if (m_device != VK_NULL_HANDLE && m_image != VK_NULL_HANDLE) {
        vkDestroyImage(
            m_device,
            m_image,
            nullptr
        );
    }

    m_image = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;

    m_extent = {};
    m_format = VK_FORMAT_UNDEFINED;
    m_usage = 0;
    m_tiling = VK_IMAGE_TILING_OPTIMAL;
    m_image_type = VK_IMAGE_TYPE_2D;
    m_mip_levels = 1;
    m_array_layers = 1;
    m_samples = VK_SAMPLE_COUNT_1_BIT;

    m_memory.reset();
}

VulkanImage::VulkanImage(VulkanImage&& other) noexcept
    :   m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
        m_image(std::exchange(other.m_image, VK_NULL_HANDLE)),
        m_extent(std::exchange(other.m_extent, VkExtent3D{})),
        m_format(std::exchange(other.m_format, VK_FORMAT_UNDEFINED)),
        m_usage(std::exchange(other.m_usage, 0)),
        m_tiling(std::exchange(other.m_tiling, VK_IMAGE_TILING_OPTIMAL)),
        m_image_type(std::exchange(other.m_image_type, VK_IMAGE_TYPE_2D)),
        m_mip_levels(std::exchange(other.m_mip_levels, 1)),
        m_array_layers(std::exchange(other.m_array_layers, 1)),
        m_samples(std::exchange(other.m_samples, VK_SAMPLE_COUNT_1_BIT)),
        m_memory(std::move(other.m_memory)) {}

VulkanImage& VulkanImage::operator=(VulkanImage&& other) noexcept {
    if (this != &other) {
        destroy();

        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_image = std::exchange(other.m_image, VK_NULL_HANDLE);
        m_extent = std::exchange(other.m_extent, VkExtent3D{});
        m_format = std::exchange(other.m_format, VK_FORMAT_UNDEFINED);
        m_usage = std::exchange(other.m_usage, 0);
        m_tiling = std::exchange(other.m_tiling, VK_IMAGE_TILING_OPTIMAL);
        m_image_type = std::exchange(other.m_image_type, VK_IMAGE_TYPE_2D);
        m_mip_levels = std::exchange(other.m_mip_levels, 1);
        m_array_layers = std::exchange(other.m_array_layers, 1);
        m_samples = std::exchange(other.m_samples, VK_SAMPLE_COUNT_1_BIT);
        m_memory = std::move(other.m_memory);
    }

    return *this;
}

VkImage VulkanImage::handle() const noexcept {
    return m_image;
}

VkExtent3D VulkanImage::extent() const noexcept {
    return m_extent;
}

VkFormat VulkanImage::format() const noexcept {
    return m_format;
}

VkImageUsageFlags VulkanImage::usage() const noexcept {
    return m_usage;
}

uint32_t VulkanImage::mip_levels() const noexcept {
    return m_mip_levels;
}

uint32_t VulkanImage::array_layers() const noexcept {
    return m_array_layers;
}

bool VulkanImage::has_usage(VkImageUsageFlags usage) const noexcept {
    return (m_usage & usage) == usage;
}

void VulkanImage::memory_barrier(
    VulkanCommandBuffer& command_buffer,
    VkPipelineStageFlags src_stage,
    VkAccessFlags src_access,
    VkPipelineStageFlags dst_stage,
    VkAccessFlags dst_access,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    VkImageAspectFlags aspect_mask,
    uint32_t base_mip_level,
    uint32_t level_count,
    uint32_t base_array_layer,
    uint32_t layer_count) const
{
    LOG_METHOD();

    logger.check(command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initialized");
    logger.check(m_image != VK_NULL_HANDLE, "Image is not initialized");
    logger.check(src_stage != 0, "Source pipeline stage mask is empty");
    logger.check(dst_stage != 0, "Destination pipeline stage mask is empty");
    logger.check(aspect_mask != 0, "Image aspect mask is empty");

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_image;

    barrier.subresourceRange.aspectMask = aspect_mask;
    barrier.subresourceRange.baseMipLevel = base_mip_level;
    barrier.subresourceRange.levelCount = level_count;
    barrier.subresourceRange.baseArrayLayer = base_array_layer;
    barrier.subresourceRange.layerCount = layer_count;

    vkCmdPipelineBarrier(
        command_buffer.handle(),
        src_stage,
        dst_stage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );
}

void VulkanImage::barrier_undefined_to_transfer_dst(
    VulkanCommandBuffer& command_buffer,
    VkImageAspectFlags aspect_mask) const
{
    LOG_METHOD();

    logger.check(
        has_usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT),
        "Image was not created with VK_IMAGE_USAGE_TRANSFER_DST_BIT"
    );

    memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        0,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        aspect_mask
    );
}

void VulkanImage::barrier_transfer_write_to_shader_read(
    VulkanCommandBuffer& command_buffer,
    VkPipelineStageFlags shader_stage,
    VkImageAspectFlags aspect_mask) const
{
    LOG_METHOD();

    logger.check(
        has_usage(VK_IMAGE_USAGE_SAMPLED_BIT) || has_usage(VK_IMAGE_USAGE_STORAGE_BIT),
        "Image was not created with shader-readable usage"
    );

    logger.check(shader_stage != 0, "Shader stage mask is empty");

    memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        shader_stage,
        VK_ACCESS_SHADER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        aspect_mask
    );
}

void VulkanImage::copy_from_buffer(
    VulkanCommandBuffer& command_buffer,
    const VulkanBuffer& src_buffer,
    VkDeviceSize buffer_offset,
    VkOffset3D image_offset,
    VkExtent3D image_extent,
    uint32_t mip_level,
    uint32_t base_array_layer,
    uint32_t layer_count,
    VkImageAspectFlags aspect_mask) const
{
    LOG_METHOD();

    logger.check(command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initialized");
    logger.check(m_image != VK_NULL_HANDLE, "Image is not initialized");
    logger.check(src_buffer.handle() != VK_NULL_HANDLE, "Source buffer is not initialized");

    logger.check(
        has_usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT),
        "Image was not created with VK_IMAGE_USAGE_TRANSFER_DST_BIT"
    );

    logger.check(
        src_buffer.has_usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT),
        "Source buffer was not created with VK_BUFFER_USAGE_TRANSFER_SRC_BIT"
    );

    if (image_extent.width == 0 || image_extent.height == 0 || image_extent.depth == 0) {
        image_extent = m_extent;
    }

    VkBufferImageCopy copy_region{};
    copy_region.bufferOffset = buffer_offset;
    copy_region.bufferRowLength = 0;
    copy_region.bufferImageHeight = 0;

    copy_region.imageSubresource.aspectMask = aspect_mask;
    copy_region.imageSubresource.mipLevel = mip_level;
    copy_region.imageSubresource.baseArrayLayer = base_array_layer;
    copy_region.imageSubresource.layerCount = layer_count;

    copy_region.imageOffset = image_offset;
    copy_region.imageExtent = image_extent;

    vkCmdCopyBufferToImage(
        command_buffer.handle(),
        src_buffer.handle(),
        m_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &copy_region
    );
}
