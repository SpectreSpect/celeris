#include "cubemap.h"

#include <algorithm>
#include <utility>

#include "../vulkan_physical_device.h"
#include "../vulkan_device.h"
#include "../vulkan_command_buffer.h"

Cubemap::Cubemap(
    VulkanImage&& image,
    VulkanImageView&& view,
    VulkanSampler&& sampler,
    VkImageLayout layout)
    :   m_image(std::move(image)),
        m_view(std::move(view)),
        m_sampler(std::move(sampler)),
        m_layout(layout) {}

Cubemap::Cubemap(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkExtent2D extent2d,
    uint32_t mip_levels)
    :   Cubemap(
            physical_device,
            device,
            extent2d,
            VK_FORMAT_R8G8B8A8_SRGB,
            mip_levels
        ) {}

Cubemap::Cubemap(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkExtent2D extent2d,
    VkFormat format,
    uint32_t mip_levels)
    :   m_image(
            create_image(
                physical_device,
                device,
                extent2d,
                format,
                mip_levels
            )
        ),
        m_view(
            device,
            m_image,
            VK_IMAGE_ASPECT_COLOR_BIT,
            0,
            m_image.mip_levels(),
            0,
            face_count,
            VK_IMAGE_VIEW_TYPE_CUBE
        ),
        m_sampler(
            device,
            create_sampler_desc(
                m_image.mip_levels(),
                VK_FILTER_LINEAR,
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                VK_SAMPLER_MIPMAP_MODE_LINEAR
            )
        ),
        m_layout(VK_IMAGE_LAYOUT_UNDEFINED) {}

VulkanImage& Cubemap::image() noexcept {
    return m_image;
}

const VulkanImage& Cubemap::image() const noexcept {
    return m_image;
}

VulkanImageView& Cubemap::view() noexcept {
    return m_view;
}

const VulkanImageView& Cubemap::view() const noexcept {
    return m_view;
}

VulkanSampler& Cubemap::sampler() noexcept {
    return m_sampler;
}

const VulkanSampler& Cubemap::sampler() const noexcept {
    return m_sampler;
}

VkExtent3D Cubemap::extent() const noexcept {
    return m_image.extent();
}

VkExtent2D Cubemap::extent2d() const noexcept {
    return m_image.extent2d();
}

VkFormat Cubemap::format() const noexcept {
    return m_image.format();
}

uint32_t Cubemap::mip_levels() const noexcept {
    return m_image.mip_levels();
}

uint32_t Cubemap::array_layers() const noexcept {
    return m_image.array_layers();
}

VkImageLayout Cubemap::layout() const noexcept {
    return m_layout;
}

VkImageLayout Cubemap::texture_layout() const noexcept {
    return m_layout;
}

VkDescriptorImageInfo Cubemap::descriptor_image_info() const noexcept {
    VkDescriptorImageInfo info{};
    info.sampler = m_sampler.handle();
    info.imageView = m_view.handle();
    info.imageLayout = m_layout;
    return info;
}

void Cubemap::set_layout(VkImageLayout layout) noexcept {
    m_layout = layout;
}

uint32_t Cubemap::calculate_mip_levels(VkExtent2D extent2d) {
    LOG_NAMED("Cubemap");

    logger.check(extent2d.width != 0, "Cubemap width is zero");
    logger.check(extent2d.height != 0, "Cubemap height is zero");
    logger.check(extent2d.width == extent2d.height, "Cubemap faces must be square");

    uint32_t max_dimension = std::max(extent2d.width, extent2d.height);

    uint32_t levels = 1;
    while (max_dimension > 1) {
        max_dimension /= 2;
        levels++;
    }

    return levels;
}

void Cubemap::transition_layout(
    VulkanCommandBuffer& command_buffer,
    VkImageLayout new_layout,
    VkPipelineStageFlags src_stage,
    VkPipelineStageFlags dst_stage,
    VkAccessFlags src_access,
    VkAccessFlags dst_access,
    uint32_t base_mip_level,
    uint32_t level_count,
    uint32_t base_array_layer,
    uint32_t layer_count)
{
    LOG_METHOD();

    logger.check(m_image.handle() != VK_NULL_HANDLE, "Cubemap image is not initialized");
    logger.check(new_layout != VK_IMAGE_LAYOUT_UNDEFINED, "New cubemap layout must not be UNDEFINED");
    logger.check(src_stage != 0, "Source stage must not be zero");
    logger.check(dst_stage != 0, "Destination stage must not be zero");

    if (level_count == 0) {
        level_count = m_image.mip_levels() - base_mip_level;
    }

    logger.check(base_mip_level < m_image.mip_levels(), "Base mip level is out of range");
    logger.check(level_count <= m_image.mip_levels() - base_mip_level, "Mip level range is out of bounds");

    logger.check(base_array_layer < m_image.array_layers(), "Base array layer is out of range");
    logger.check(layer_count <= m_image.array_layers() - base_array_layer, "Array layer range is out of bounds");

    m_image.memory_barrier(
        command_buffer,
        src_stage,
        src_access,
        dst_stage,
        dst_access,
        m_layout,
        new_layout,
        VK_IMAGE_ASPECT_COLOR_BIT,
        base_mip_level,
        level_count,
        base_array_layer,
        layer_count
    );

    m_layout = new_layout;
}

void Cubemap::generate_mipmaps(VulkanCommandBuffer& command_buffer)
{
    LOG_METHOD();

    logger.check(image().handle() != VK_NULL_HANDLE, "Cubemap image is not initialized");
    logger.check(mip_levels() != 0, "Cubemap mip levels count is zero");
    logger.check(array_layers() == Cubemap::face_count, "Cubemap must have 6 faces");

    logger.check(
        image().has_usage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
        "Cubemap image must have VK_IMAGE_USAGE_TRANSFER_SRC_BIT for mip generation"
    );

    logger.check(
        image().has_usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT),
        "Cubemap image must have VK_IMAGE_USAGE_TRANSFER_DST_BIT for mip generation"
    );

    if (mip_levels() == 1) {
        image().memory_barrier(
            command_buffer,

            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,

            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT,

            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

            VK_IMAGE_ASPECT_COLOR_BIT,

            0,
            1,

            0,
            Cubemap::face_count
        );

        set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        return;
    }

    int32_t mip_width = static_cast<int32_t>(extent().width);
    int32_t mip_height = static_cast<int32_t>(extent().height);

    for (uint32_t mip = 1; mip < mip_levels(); mip++) {
        VkImageLayout previous_mip_old_layout =
            mip == 1
                ? VK_IMAGE_LAYOUT_GENERAL
                : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        VkPipelineStageFlags previous_mip_old_stage =
            mip == 1
                ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                : VK_PIPELINE_STAGE_TRANSFER_BIT;

        VkAccessFlags previous_mip_old_access =
            mip == 1
                ? VK_ACCESS_SHADER_WRITE_BIT
                : VK_ACCESS_TRANSFER_WRITE_BIT;

        // Previous mip becomes the blit source.
        image().memory_barrier(
            command_buffer,

            previous_mip_old_stage,
            previous_mip_old_access,

            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,

            previous_mip_old_layout,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,

            VK_IMAGE_ASPECT_COLOR_BIT,

            mip - 1,
            1,

            0,
            Cubemap::face_count
        );

        // Current mip becomes the blit destination.
        image().memory_barrier(
            command_buffer,

            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            0,

            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,

            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,

            VK_IMAGE_ASPECT_COLOR_BIT,

            mip,
            1,

            0,
            Cubemap::face_count
        );

        int32_t next_mip_width = std::max(mip_width / 2, 1);
        int32_t next_mip_height = std::max(mip_height / 2, 1);

        VkImageBlit blit{};
        blit.srcOffsets[0] = VkOffset3D{0, 0, 0};
        blit.srcOffsets[1] = VkOffset3D{mip_width, mip_height, 1};

        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = mip - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = Cubemap::face_count;

        blit.dstOffsets[0] = VkOffset3D{0, 0, 0};
        blit.dstOffsets[1] = VkOffset3D{next_mip_width, next_mip_height, 1};

        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = mip;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = Cubemap::face_count;

        vkCmdBlitImage(
            command_buffer.handle(),

            image().handle(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,

            image().handle(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,

            1,
            &blit,

            VK_FILTER_LINEAR
        );

        // Previous mip is finished. Make it shader-readable.
        image().memory_barrier(
            command_buffer,

            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,

            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT,

            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

            VK_IMAGE_ASPECT_COLOR_BIT,

            mip - 1,
            1,

            0,
            Cubemap::face_count
        );

        mip_width = next_mip_width;
        mip_height = next_mip_height;
    }

    // Last mip was only used as TRANSFER_DST, so transition it too.
    image().memory_barrier(
        command_buffer,

        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,

        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT,

        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

        VK_IMAGE_ASPECT_COLOR_BIT,

        mip_levels() - 1,
        1,

        0,
        Cubemap::face_count
    );

    set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

VulkanImage Cubemap::create_image(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkExtent2D extent2d,
    VkFormat format,
    uint32_t mip_levels)
{
    LOG_NAMED("Cubemap");

    logger.check(physical_device.handle() != VK_NULL_HANDLE, "Physical device is not initialized");
    logger.check(device.handle() != VK_NULL_HANDLE, "Device is not initialized");

    logger.check(extent2d.width != 0, "Cubemap width is zero");
    logger.check(extent2d.height != 0, "Cubemap height is zero");
    logger.check(extent2d.width == extent2d.height, "Cubemap faces must be square");

    logger.check(format != VK_FORMAT_UNDEFINED, "Cubemap format is undefined");

    if (mip_levels == 0) {
        mip_levels = calculate_mip_levels(extent2d);
    }

    logger.check(mip_levels != 0, "Cubemap mip levels count is zero");

    VkImageUsageFlags usage =
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT;

    if (mip_levels > 1) {
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    return VulkanImage(
        physical_device,
        device,
        VkExtent3D{extent2d.width, extent2d.height, 1},
        format,
        usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_TYPE_2D,
        mip_levels,
        face_count,
        VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
    );
}

VkSamplerCreateInfo Cubemap::create_sampler_desc(
    uint32_t mip_levels,
    VkFilter filter,
    VkSamplerAddressMode address_mode,
    VkSamplerMipmapMode mipmap_mode)
{
    LOG_NAMED("Cubemap");

    logger.check(mip_levels != 0, "Sampler mip levels count is zero");

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    sampler_info.magFilter = filter;
    sampler_info.minFilter = filter;

    sampler_info.addressModeU = address_mode;
    sampler_info.addressModeV = address_mode;
    sampler_info.addressModeW = address_mode;

    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1.0f;

    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;

    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;

    sampler_info.mipmapMode = mipmap_mode;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = static_cast<float>(mip_levels - 1);

    return sampler_info;
}