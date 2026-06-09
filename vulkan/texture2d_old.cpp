#include "texture2d.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

#include "video_buffer.h"
#include "../vulkan_engine.h"
#include "vulkan_utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"

// Put STB_IMAGE_IMPLEMENTATION in exactly one .cpp in your whole project.


Texture2D::Texture2D(VulkanEngine& engine,
                 int width, int height, const void* initial_data,
                 Wrap wrap, MagFilter mag_filter, MinFilter min_filter, bool srgb) {
    create(engine, width, height, initial_data, wrap, mag_filter, min_filter, srgb);
}

Texture2D::Texture2D(VulkanEngine& engine,
                 const std::string& filepath,
                 Wrap wrap, MagFilter mag_filter, MinFilter min_filter,
                 bool srgb, bool flipY) {
    loadFromFile(engine, filepath, wrap, mag_filter, min_filter, srgb, flipY);
}

Texture2D::~Texture2D() {
    destroy();
}

void Texture2D::destroy() {
    if (device && sampler != VK_NULL_HANDLE) {
        vkDestroySampler(*device, sampler, nullptr);
        sampler = VK_NULL_HANDLE;
    }

    if (device && view != VK_NULL_HANDLE) {
        vkDestroyImageView(*device, view, nullptr);
        view = VK_NULL_HANDLE;
    }

    if (device && image != VK_NULL_HANDLE) {
        vkDestroyImage(*device, image, nullptr);
        image = VK_NULL_HANDLE;
    }

    if (device && memory != VK_NULL_HANDLE) {
        vkFreeMemory(*device, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }

    width = 0;
    height = 0;
    mip_levels = 1;
}

VkDescriptorImageInfo Texture2D::descriptor_info() const {
    VkDescriptorImageInfo info{};
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.imageView = view;
    info.sampler = sampler;
    return info;
}

VkSamplerAddressMode Texture2D::to_vk_wrap(Wrap wrap) {
    switch (wrap) {
        case Wrap::ClampToEdge:    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case Wrap::Repeat:         return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case Wrap::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        default:                   return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

VkFilter Texture2D::to_vk_mag_filter(MagFilter filter) {
    switch (filter) {
        case MagFilter::Nearest: return VK_FILTER_NEAREST;
        case MagFilter::Linear:  return VK_FILTER_LINEAR;
        default:                 return VK_FILTER_LINEAR;
    }
}

Texture2D::MinFilterInfo Texture2D::to_vk_min_filter(MinFilter filter) {
    switch (filter) {
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
            return {VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, true};
    }
}

uint32_t Texture2D::compute_mip_levels(uint32_t w, uint32_t h) {
    return 1u + static_cast<uint32_t>(std::floor(std::log2(static_cast<float>(std::max(w, h)))));
}

VkCommandBuffer Texture2D::begin_single_time_commands(VulkanEngine& engine) {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = engine.commandPool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vulkan_utils::vk_check(
        vkAllocateCommandBuffers(engine.device, &alloc_info, &cmd),
        "vkAllocateCommandBuffers(Texture2D)"
    );

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vulkan_utils::vk_check(
        vkBeginCommandBuffer(cmd, &begin_info),
        "vkBeginCommandBuffer(Texture2D)"
    );

    return cmd;
}

void Texture2D::end_single_time_commands(VulkanEngine& engine, VkCommandBuffer cmd) {
    vulkan_utils::vk_check(
        vkEndCommandBuffer(cmd),
        "vkEndCommandBuffer(Texture2D)"
    );

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    vulkan_utils::vk_check(
        vkQueueSubmit(engine.graphicsQueue, 1, &submit_info, VK_NULL_HANDLE),
        "vkQueueSubmit(Texture2D)"
    );

    vulkan_utils::vk_check(
        vkQueueWaitIdle(engine.graphicsQueue),
        "vkQueueWaitIdle(Texture2D)"
    );

    vkFreeCommandBuffers(engine.device, engine.commandPool, 1, &cmd);
}

void Texture2D::create_image(VulkanEngine& engine, VkFormat image_format, VkImageUsageFlags usage) {
    format = image_format;
    device = &engine.device;
    physical_device = engine.physicalDevice;

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = mip_levels;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = usage;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vulkan_utils::vk_check(
        vkCreateImage(engine.device, &image_info, nullptr, &image),
        "vkCreateImage(Texture2D)"
    );

    VkMemoryRequirements mem_requirements{};
    vkGetImageMemoryRequirements(engine.device, image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = vulkan_utils::find_memory_type(
        engine.physicalDevice,
        mem_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    vulkan_utils::vk_check(
        vkAllocateMemory(engine.device, &alloc_info, nullptr, &memory),
        "vkAllocateMemory(Texture2D)"
    );

    vulkan_utils::vk_check(
        vkBindImageMemory(engine.device, image, memory, 0),
        "vkBindImageMemory(Texture2D)"
    );
}

void Texture2D::create_image_view() {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = mip_levels;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    vulkan_utils::vk_check(
        vkCreateImageView(*device, &view_info, nullptr, &view),
        "vkCreateImageView(Texture2D)"
    );
}

void Texture2D::create_sampler(Wrap wrap, MagFilter mag_filter, MinFilter min_filter) {
    const auto min_info = to_vk_min_filter(min_filter);

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = to_vk_mag_filter(mag_filter);
    sampler_info.minFilter = min_info.min_filter;
    sampler_info.mipmapMode = min_info.mipmap_mode;
    sampler_info.addressModeU = to_vk_wrap(wrap);
    sampler_info.addressModeV = to_vk_wrap(wrap);
    sampler_info.addressModeW = to_vk_wrap(wrap);
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
        "vkCreateSampler(Texture2D)"
    );
}

void Texture2D::transition_image_layout(VulkanEngine& engine,
                                        VkImageLayout new_layout,
                                        uint32_t base_mip_level,
                                        uint32_t level_count) {

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
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags src_stage;
    VkPipelineStageFlags dst_stage;

    if (layout == VK_IMAGE_LAYOUT_UNDEFINED &&
        new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    } else if (layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
            new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    } else if (layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
            new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    } else if (layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
            new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    } else if (layout == VK_IMAGE_LAYOUT_UNDEFINED &&
            new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    } else if (layout == VK_IMAGE_LAYOUT_UNDEFINED &&
            new_layout == VK_IMAGE_LAYOUT_GENERAL) {
        // First use as storage image in compute
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    } else if (layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
            new_layout == VK_IMAGE_LAYOUT_GENERAL) {
        // Was sampled in fragment shader, now will be written again in compute
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dst_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    } else if (layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
            new_layout == VK_IMAGE_LAYOUT_GENERAL) {
        // Uploaded/copied into image, now compute will write/read it
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    } else if (layout == VK_IMAGE_LAYOUT_GENERAL &&
            new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        // Compute wrote the image, now fragment shader will sample it
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    } else {
        throw std::runtime_error("Unsupported Texture2D layout transition");
    }

    vkCmdPipelineBarrier(
        cmd,
        src_stage,
        dst_stage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    this->layout = new_layout;

    end_single_time_commands(engine, cmd);
}

void Texture2D::transition_to_general_layout(VulkanEngine& engine) {
    transition_image_layout(engine, VK_IMAGE_LAYOUT_GENERAL, 0, mip_levels);
}

void Texture2D::transition_to_shader_read_only_layout(VulkanEngine& engine) {
    transition_image_layout(engine, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, mip_levels);
}

void Texture2D::copy_buffer_to_image(VulkanEngine& engine, VkBuffer buffer) {
    VkCommandBuffer cmd = begin_single_time_commands(engine);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(
        cmd,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    end_single_time_commands(engine, cmd);
}

void Texture2D::generate_mipmaps(VulkanEngine& engine) {
    VkFormatProperties format_properties{};
    vkGetPhysicalDeviceFormatProperties(engine.physicalDevice, format, &format_properties);

    if (!(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("Texture2D format does not support linear blitting for mipmaps");
    }

    VkCommandBuffer cmd = begin_single_time_commands(engine);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mip_width = static_cast<int32_t>(width);
    int32_t mip_height = static_cast<int32_t>(height);

    for (uint32_t i = 1; i < mip_levels; ++i) {
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
            1, &barrier
        );

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mip_width, mip_height, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;

        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {
            std::max(1, mip_width / 2),
            std::max(1, mip_height / 2),
            1
        };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(
            cmd,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR
        );

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
            1, &barrier
        );

        if (mip_width > 1) mip_width /= 2;
        if (mip_height > 1) mip_height /= 2;
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
        1, &barrier
    );

    end_single_time_commands(engine, cmd);
}

void Texture2D::create(VulkanEngine& engine,
                     int w, int h, const void* initial_data,
                     Wrap wrap, MagFilter mag_filter, MinFilter min_filter, bool srgb) {
    destroy();

    if (w <= 0 || h <= 0) {
        throw std::runtime_error("Texture2D dimensions must be positive");
    }

    width = static_cast<uint32_t>(w);
    height = static_cast<uint32_t>(h);

    const auto min_info = to_vk_min_filter(min_filter);
    mip_levels = (initial_data && min_info.needs_mipmaps)
        ? compute_mip_levels(width, height)
        : 1;

    const VkFormat image_format = srgb
        ? VK_FORMAT_R8G8B8A8_SRGB
        : VK_FORMAT_R8G8B8A8_UNORM;

    VkImageUsageFlags usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (mip_levels > 1) {
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    create_image(engine, image_format, usage);

    if (initial_data) {
        const VkDeviceSize image_size = static_cast<VkDeviceSize>(width) * height * 4;

        VideoBuffer staging(
            engine,
            image_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        staging.update_data(const_cast<void*>(initial_data), image_size);

        layout = VK_IMAGE_LAYOUT_UNDEFINED;
        transition_image_layout(engine,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                0, mip_levels);

        copy_buffer_to_image(engine, staging.buffer);

        if (mip_levels > 1) {
            generate_mipmaps(engine);
        } else {
            layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            transition_image_layout(engine,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    0, 1);
        }

        // vkDestroyBuffer(engine.device, staging.buffer, nullptr);
        // vkFreeMemory(engine.device, staging.buffer_memory, nullptr);
    } else {
        layout = VK_IMAGE_LAYOUT_UNDEFINED;
        transition_image_layout(engine,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                0, 1);
    }

    create_image_view();
    create_sampler(wrap, mag_filter, min_filter);
}

bool Texture2D::loadFromFile(VulkanEngine& engine,
                           const std::string& filepath,
                           Wrap wrap, MagFilter mag_filter, MinFilter min_filter,
                           bool srgb, bool flipY) {
    stbi_set_flip_vertically_on_load(flipY ? 1 : 0);

    int w = 0;
    int h = 0;
    int channels = 0;

    stbi_uc* pixels = stbi_load(filepath.c_str(), &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels) {
        return false;
    }

    try {
        create(engine, w, h, pixels, wrap, mag_filter, min_filter, srgb);
    } catch (...) {
        stbi_image_free(pixels);
        throw;
    }

    stbi_image_free(pixels);
    return true;
}