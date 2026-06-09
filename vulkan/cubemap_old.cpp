#include "cubemap.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "video_buffer.h"
#include "../vulkan_engine.h"
#include "vulkan_utils.h"
#include "../stb_image.h"

Cubemap::Cubemap(VulkanEngine &engine,
                 const std::array<std::string, 6> &facepaths,
                 MagFilter mag_filter,
                 MinFilter min_filter,
                 bool srgb,
                 bool flipY)
{
    loadFromFaces(engine, facepaths, mag_filter, min_filter, srgb, flipY);
}

Cubemap::~Cubemap()
{
    destroy();
}

Cubemap::Cubemap(Cubemap &&other) noexcept
{
    image = other.image;
    memory = other.memory;
    view = other.view;
    sampler = other.sampler;
    device = other.device;
    physical_device = other.physical_device;
    format = other.format;
    size = other.size;
    mip_levels = other.mip_levels;

    other.image = VK_NULL_HANDLE;
    other.memory = VK_NULL_HANDLE;
    other.view = VK_NULL_HANDLE;
    other.sampler = VK_NULL_HANDLE;
    other.device = nullptr;
    other.physical_device = VK_NULL_HANDLE;
    other.format = VK_FORMAT_R8G8B8A8_UNORM;
    other.size = 0;
    other.mip_levels = 1;
}

Cubemap &Cubemap::operator=(Cubemap &&other) noexcept
{
    if (this != &other)
    {
        destroy();

        image = other.image;
        memory = other.memory;
        view = other.view;
        sampler = other.sampler;
        device = other.device;
        physical_device = other.physical_device;
        format = other.format;
        size = other.size;
        mip_levels = other.mip_levels;

        other.image = VK_NULL_HANDLE;
        other.memory = VK_NULL_HANDLE;
        other.view = VK_NULL_HANDLE;
        other.sampler = VK_NULL_HANDLE;
        other.device = nullptr;
        other.physical_device = VK_NULL_HANDLE;
        other.format = VK_FORMAT_R8G8B8A8_UNORM;
        other.size = 0;
        other.mip_levels = 1;
    }
    return *this;
}

void Cubemap::destroy()
{
    if (device && sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(*device, sampler, nullptr);
        sampler = VK_NULL_HANDLE;
    }

    if (device && view != VK_NULL_HANDLE)
    {
        vkDestroyImageView(*device, view, nullptr);
        view = VK_NULL_HANDLE;
    }

    if (device && image != VK_NULL_HANDLE)
    {
        vkDestroyImage(*device, image, nullptr);
        image = VK_NULL_HANDLE;
    }

    if (device && memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(*device, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }

    device = nullptr;
    physical_device = VK_NULL_HANDLE;
    format = VK_FORMAT_R8G8B8A8_UNORM;
    size = 0;
    mip_levels = 1;
}

VkDescriptorImageInfo Cubemap::descriptor_info() const
{
    VkDescriptorImageInfo info{};
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.imageView = view;
    info.sampler = sampler;
    return info;
}

uint32_t Cubemap::compute_mip_levels(uint32_t s)
{
    return 1u + static_cast<uint32_t>(std::floor(std::log2(static_cast<float>(s))));
}

VkFilter Cubemap::to_vk_mag_filter(MagFilter filter)
{
    switch (filter)
    {
    case MagFilter::Nearest:
        return VK_FILTER_NEAREST;
    case MagFilter::Linear:
        return VK_FILTER_LINEAR;
    default:
        return VK_FILTER_LINEAR;
    }
}

Cubemap::MinFilterInfo Cubemap::to_vk_min_filter(MinFilter filter)
{
    switch (filter)
    {
    case MinFilter::Nearest:
        return {VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, false};
    case MinFilter::Linear:
        return {VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, false};
    case MinFilter::NearestMipmapNearest:
        return {VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, true};
    case MinFilter::LinearMipmapNearest:
        return {VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, true};
    case MinFilter::NearestMipmapLinear:
        return {VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_LINEAR, true};
    case MinFilter::LinearMipmapLinear:
        return {VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, true};
    default:
        return {VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, false};
    }
}

VkCommandBuffer Cubemap::begin_single_time_commands(VulkanEngine &engine)
{
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = engine.commandPool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vulkan_utils::vk_check(
        vkAllocateCommandBuffers(engine.device, &alloc_info, &cmd),
        "vkAllocateCommandBuffers(cubemap)");

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vulkan_utils::vk_check(
        vkBeginCommandBuffer(cmd, &begin_info),
        "vkBeginCommandBuffer(cubemap)");

    return cmd;
}

void Cubemap::end_single_time_commands(VulkanEngine &engine, VkCommandBuffer cmd)
{
    vulkan_utils::vk_check(
        vkEndCommandBuffer(cmd),
        "vkEndCommandBuffer(cubemap)");

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    vulkan_utils::vk_check(
        vkQueueSubmit(engine.graphicsQueue, 1, &submit_info, VK_NULL_HANDLE),
        "vkQueueSubmit(cubemap)");

    vulkan_utils::vk_check(
        vkQueueWaitIdle(engine.graphicsQueue),
        "vkQueueWaitIdle(cubemap)");

    vkFreeCommandBuffers(engine.device, engine.commandPool, 1, &cmd);
}

void Cubemap::create_image(VulkanEngine &engine, VkFormat image_format, VkImageUsageFlags usage)
{
    device = &engine.device;
    physical_device = engine.physicalDevice;
    format = image_format;

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = size;
    image_info.extent.height = size;
    image_info.extent.depth = 1;
    image_info.mipLevels = mip_levels;
    image_info.arrayLayers = 6;
    image_info.format = format;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vulkan_utils::vk_check(
        vkCreateImage(engine.device, &image_info, nullptr, &image),
        "vkCreateImage(cubemap)");

    VkMemoryRequirements mem_requirements{};
    vkGetImageMemoryRequirements(engine.device, image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = vulkan_utils::find_memory_type(
        engine.physicalDevice,
        mem_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vulkan_utils::vk_check(
        vkAllocateMemory(engine.device, &alloc_info, nullptr, &memory),
        "vkAllocateMemory(cubemap)");

    vulkan_utils::vk_check(
        vkBindImageMemory(engine.device, image, memory, 0),
        "vkBindImageMemory(cubemap)");
}

void Cubemap::create_image_view()
{
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = mip_levels;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 6;

    vulkan_utils::vk_check(
        vkCreateImageView(*device, &view_info, nullptr, &view),
        "vkCreateImageView(cubemap)");
}

void Cubemap::create_sampler(MagFilter mag_filter, MinFilter min_filter)
{
    const auto min_info = to_vk_min_filter(min_filter);

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = to_vk_mag_filter(mag_filter);
    sampler_info.minFilter = min_info.min_filter;
    sampler_info.mipmapMode = min_info.mipmap_mode;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1.0f;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = (mip_levels > 1) ? static_cast<float>(mip_levels) : 0.0f;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;

    vulkan_utils::vk_check(
        vkCreateSampler(*device, &sampler_info, nullptr, &sampler),
        "vkCreateSampler(cubemap)");
}

void Cubemap::transition_image_layout(VulkanEngine &engine,
                                      VkImageLayout new_layout,
                                      uint32_t base_mip_level,
                                      uint32_t level_count)
{
    VkCommandBuffer cmd = begin_single_time_commands(engine);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = base_mip_level;
    barrier.subresourceRange.levelCount = level_count;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;

    VkPipelineStageFlags src_stage;
    VkPipelineStageFlags dst_stage;

    // if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
    //     new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    //     barrier.srcAccessMask = 0;
    //     barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    //     src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    //     dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    // } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
    //            new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    //     barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    //     barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    //     src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    //     dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    // } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
    //            new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
    //     barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    //     barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    //     src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    //     dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    // } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
    //            new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    //     barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    //     barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    //     src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    //     dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    // } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
    //            new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    //     barrier.srcAccessMask = 0;
    //     barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    //     src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    //     dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    // } else {
    //     throw std::runtime_error("Unsupported cubemap layout transition");
    // }

    if (layout == VK_IMAGE_LAYOUT_UNDEFINED &&
        new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (layout == VK_IMAGE_LAYOUT_UNDEFINED &&
             new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (layout == VK_IMAGE_LAYOUT_UNDEFINED &&
             new_layout == VK_IMAGE_LAYOUT_GENERAL)
    {
        // First use as storage image in compute
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else if (layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_GENERAL)
    {
        // Was sampled in fragment shader, now will be written again in compute
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dst_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else if (layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_GENERAL)
    {
        // Uploaded/copied into image, now compute will write/read it
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else if (layout == VK_IMAGE_LAYOUT_GENERAL &&
             new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        // Compute wrote the image, now fragment shader will sample it
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        throw std::runtime_error("Unsupported Texture2D layout transition");
    }

    vkCmdPipelineBarrier(
        cmd,
        src_stage,
        dst_stage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    this->layout = new_layout;

    end_single_time_commands(engine, cmd);
}

void Cubemap::transition_to_general_layout(VulkanEngine &engine)
{
    transition_image_layout(engine, VK_IMAGE_LAYOUT_GENERAL, 0, mip_levels);
}

void Cubemap::transition_to_shader_read_only_layout(VulkanEngine &engine)
{
    transition_image_layout(engine, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, mip_levels);
}

void Cubemap::copy_buffer_to_cubemap(VulkanEngine &engine, VkBuffer buffer)
{
    VkCommandBuffer cmd = begin_single_time_commands(engine);

    std::array<VkBufferImageCopy, 6> regions{};

    const VkDeviceSize face_size_bytes = static_cast<VkDeviceSize>(size) * size * 4;

    for (uint32_t face = 0; face < 6; ++face)
    {
        auto &region = regions[face];
        region.bufferOffset = face_size_bytes * face;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = face;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {size, size, 1};
    }

    vkCmdCopyBufferToImage(
        cmd,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        static_cast<uint32_t>(regions.size()),
        regions.data());

    end_single_time_commands(engine, cmd);
}

void Cubemap::generate_mipmaps(VulkanEngine &engine)
{
    VkFormatProperties format_properties{};
    vkGetPhysicalDeviceFormatProperties(engine.physicalDevice, format, &format_properties);

    if (!(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    {
        throw std::runtime_error("Cubemap format does not support linear blitting for mipmaps");
    }

    VkCommandBuffer cmd = begin_single_time_commands(engine);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;
    barrier.subresourceRange.levelCount = 1;

    int32_t mip_size = static_cast<int32_t>(size);

    for (uint32_t i = 1; i < mip_levels; ++i)
    {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        std::array<VkImageBlit, 6> blits{};

        for (uint32_t face = 0; face < 6; ++face)
        {
            auto &blit = blits[face];

            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mip_size, mip_size, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = face;
            blit.srcSubresource.layerCount = 1;

            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = {
                std::max(1, mip_size / 2),
                std::max(1, mip_size / 2),
                1};
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = face;
            blit.dstSubresource.layerCount = 1;
        }

        for (const auto &blit : blits)
        {
            vkCmdBlitImage(
                cmd,
                image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit,
                VK_FILTER_LINEAR);
        }

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        if (mip_size > 1)
            mip_size /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mip_levels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    end_single_time_commands(engine, cmd);
}

void Cubemap::createEmpty(VulkanEngine &engine,
                          int face_size,
                          VkFormat image_format,
                          MagFilter mag_filter,
                          MinFilter min_filter)
{
    destroy();

    if (face_size <= 0)
    {
        throw std::runtime_error("Cubemap face size must be positive");
    }

    size = static_cast<uint32_t>(face_size);

    const auto min_info = to_vk_min_filter(min_filter);
    mip_levels = min_info.needs_mipmaps ? compute_mip_levels(size) : 1;

    VkImageUsageFlags usage =
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    create_image(engine, image_format, usage);
    layout = VK_IMAGE_LAYOUT_UNDEFINED;
    transition_image_layout(engine, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, mip_levels);
    create_image_view();
    create_sampler(mag_filter, min_filter);
}

bool Cubemap::loadFromFaces(VulkanEngine &engine,
                            const std::array<std::string, 6> &facepaths,
                            MagFilter mag_filter,
                            MinFilter min_filter,
                            bool srgb,
                            bool flipY)
{
    destroy();

    this->device = &engine.device;

    stbi_set_flip_vertically_on_load(flipY ? 1 : 0);

    int w = 0;
    int h = 0;
    int channels = 0;

    std::array<stbi_uc *, 6> pixels{};
    for (int i = 0; i < 6; ++i)
    {
        int face_w = 0;
        int face_h = 0;
        int face_channels = 0;

        pixels[i] = stbi_load(facepaths[i].c_str(), &face_w, &face_h, &face_channels, STBI_rgb_alpha);
        if (!pixels[i])
        {
            for (int j = 0; j < i; ++j)
            {
                stbi_image_free(pixels[j]);
            }
            stbi_set_flip_vertically_on_load(0);
            return false;
        }

        if (i == 0)
        {
            w = face_w;
            h = face_h;
            channels = 4;
            if (w != h)
            {
                for (int j = 0; j <= i; ++j)
                {
                    stbi_image_free(pixels[j]);
                }
                stbi_set_flip_vertically_on_load(0);
                return false;
            }
        }
        else
        {
            if (face_w != w || face_h != h || face_w != face_h)
            {
                for (int j = 0; j <= i; ++j)
                {
                    stbi_image_free(pixels[j]);
                }
                stbi_set_flip_vertically_on_load(0);
                return false;
            }
        }
    }

    size = static_cast<uint32_t>(w);

    const auto min_info = to_vk_min_filter(min_filter);
    mip_levels = min_info.needs_mipmaps ? compute_mip_levels(size) : 1;
    format = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

    const VkDeviceSize face_size_bytes = static_cast<VkDeviceSize>(w) * h * 4;
    const VkDeviceSize total_size = face_size_bytes * 6;

    std::vector<stbi_uc> combined(static_cast<size_t>(total_size));
    for (int face = 0; face < 6; ++face)
    {
        std::memcpy(combined.data() + face * face_size_bytes, pixels[face], static_cast<size_t>(face_size_bytes));
        stbi_image_free(pixels[face]);
    }

    stbi_set_flip_vertically_on_load(0);

    VkImageUsageFlags usage =
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (mip_levels > 1)
    {
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    create_image(engine, format, usage);

    VideoBuffer staging(
        engine,
        total_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    staging.update_data(combined.data(), total_size);

    layout = VK_IMAGE_LAYOUT_UNDEFINED;
    transition_image_layout(engine,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            0, mip_levels);

    copy_buffer_to_cubemap(engine, staging.buffer);

    if (mip_levels > 1)
    {
        generate_mipmaps(engine);
    }
    else
    {
        layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        transition_image_layout(engine,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                0, 1);
    }

    // vkDestroyBuffer(engine.device, staging.buffer, nullptr);
    // vkFreeMemory(engine.device, staging.buffer_memory, nullptr);

    create_image_view();
    create_sampler(mag_filter, min_filter);

    return true;
}