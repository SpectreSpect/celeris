#include "vulkan_texture_2d.h"

#include <algorithm>
#include <utility>

#include "../vulkan_physical_device.h"
#include "../vulkan_device.h"

VulkanTexture2D::VulkanTexture2D(
    VulkanImage&& image,
    VulkanImageView&& view,
    VulkanSampler&& sampler,
    VkImageLayout layout)
    :   m_image(std::move(image)),
        m_view(std::move(view)),
        m_sampler(std::move(sampler)),
        m_layout(layout) {}

VulkanTexture2D::VulkanTexture2D(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    const VulkanTexture2DDesc& desc)
    :   m_image(
            create_image(
                physical_device,
                device,
                desc
            )
        ),
        m_view(
            create_view(
                device,
                m_image,
                desc
            )
        ),
        m_sampler(
            device,
            create_sampler_desc(
                desc,
                m_image.mip_levels()
            )
        ),
        m_layout(desc.initial_layout) {}

VulkanTexture2D::VulkanTexture2D(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkExtent2D extent2d,
    uint32_t mip_levels)
    :   VulkanTexture2D(
            physical_device,
            device,
            [&]() {
                VulkanTexture2DDesc desc = deafult_desc(extent2d);
                desc.mip_levels = mip_levels;
                return desc;
            }()
        ) {}

VulkanTexture2D::VulkanTexture2D(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkExtent2D extent2d,
    VkFormat format,
    uint32_t mip_levels)
    :   VulkanTexture2D(
            physical_device,
            device,
            [&]() {
                VulkanTexture2DDesc desc = deafult_desc(extent2d);
                desc.format = format;
                desc.mip_levels = mip_levels;
                return desc;
            }()
        ) {}

VulkanImage& VulkanTexture2D::image() noexcept {
    return m_image;
}

const VulkanImage& VulkanTexture2D::image() const noexcept {
    return m_image;
}

VulkanImageView& VulkanTexture2D::view() noexcept {
    return m_view;
}

const VulkanImageView& VulkanTexture2D::view() const noexcept {
    return m_view;
}

VulkanSampler& VulkanTexture2D::sampler() noexcept {
    return m_sampler;
}

const VulkanSampler& VulkanTexture2D::sampler() const noexcept {
    return m_sampler;
}

VkExtent3D VulkanTexture2D::extent() const noexcept {
    return m_image.extent();
}

VkExtent2D VulkanTexture2D::extent2d() const noexcept {
    return m_image.extent2d();
}

VkFormat VulkanTexture2D::format() const noexcept {
    return m_image.format();
}

uint32_t VulkanTexture2D::mip_levels() const noexcept {
    return m_image.mip_levels();
}

VkImageLayout VulkanTexture2D::layout() const noexcept {
    return m_layout;
}

VkImageLayout VulkanTexture2D::texture_layout() const noexcept {
    return m_layout;
}

VkDescriptorImageInfo VulkanTexture2D::descriptor_image_info() const noexcept {
    VkDescriptorImageInfo info{};
    info.sampler = m_sampler.handle();
    info.imageView = m_view.handle();
    info.imageLayout = m_layout;
    return info;
}

void VulkanTexture2D::set_layout(VkImageLayout layout) noexcept {
    m_layout = layout;
}

VulkanTexture2DDesc VulkanTexture2D::deafult_desc(VkExtent2D extent2d) {
    VulkanTexture2DDesc desc{};
    desc.extent2d = extent2d;
    return desc;
}

VulkanTexture2DDesc VulkanTexture2D::default_desc(VkExtent2D extent2d) {
    return deafult_desc(extent2d);
}

uint32_t VulkanTexture2D::calculate_mip_levels(VkExtent2D extent2d) {
    LOG_NAMED("VulkanTexture2D");

    logger.check(extent2d.width != 0, "Texture width is zero");
    logger.check(extent2d.height != 0, "Texture height is zero");

    uint32_t max_dimension = std::max(extent2d.width, extent2d.height);

    uint32_t levels = 1;
    while (max_dimension > 1) {
        max_dimension /= 2;
        levels++;
    }

    return levels;
}

void VulkanTexture2D::transition_layout(
    VulkanCommandBuffer& command_buffer,
    VkImageLayout new_layout,
    VkPipelineStageFlags src_stage,
    VkPipelineStageFlags dst_stage,
    VkAccessFlags src_access,
    VkAccessFlags dst_access,
    uint32_t base_mip_level,
    uint32_t level_count)
{
    LOG_METHOD();

    logger.check(m_image.handle() != VK_NULL_HANDLE, "Texture image is not initialized");
    logger.check(new_layout != VK_IMAGE_LAYOUT_UNDEFINED, "New texture layout must not be UNDEFINED");
    logger.check(src_stage != 0, "Source stage must not be zero");
    logger.check(dst_stage != 0, "Destination stage must not be zero");

    if (level_count == 0) {
        level_count = m_image.mip_levels() - base_mip_level;
    }

    logger.check(base_mip_level < m_image.mip_levels(), "Base mip level is out of range");
    logger.check(level_count <= m_image.mip_levels() - base_mip_level, "Mip level range is out of bounds");

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
        0,
        1
    );

    m_layout = new_layout;
}

VulkanImage VulkanTexture2D::create_image(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    const VulkanTexture2DDesc& desc)
{
    LOG_NAMED("VulkanTexture2D");

    logger.check(physical_device.handle() != VK_NULL_HANDLE, "Physical device is not initialized");
    logger.check(device.handle() != VK_NULL_HANDLE, "Device is not initialized");

    logger.check(desc.extent2d.width != 0, "Texture width is zero");
    logger.check(desc.extent2d.height != 0, "Texture height is zero");

    logger.check(desc.format != VK_FORMAT_UNDEFINED, "Texture format is undefined");
    logger.check(desc.usage != 0, "Texture usage flags must not be zero");
    uint32_t mip_levels = desc.mip_levels;
    if (mip_levels == 0) {
        mip_levels = calculate_mip_levels(desc.extent2d);
    }

    logger.check(mip_levels != 0, "Texture mip levels count is zero");

    VkImageUsageFlags usage = desc.usage;

    if (mip_levels > 1) {
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    return VulkanImage(
        physical_device,
        device,
        VkExtent3D{desc.extent2d.width, desc.extent2d.height, 1},
        desc.format,
        usage,
        desc.memory_properties,
        desc.tiling,
        VK_IMAGE_TYPE_2D,
        mip_levels,
        1,
        desc.samples,
        desc.initial_layout,
        desc.image_flags
    );
}

VulkanImageView VulkanTexture2D::create_view(
    const VulkanDevice& device,
    const VulkanImage& image,
    const VulkanTexture2DDesc& desc)
{
    LOG_NAMED("VulkanTexture2D");

    logger.check(image.handle() != VK_NULL_HANDLE, "Texture image is not initialized");
    logger.check(desc.view_aspect_mask != 0, "Texture view aspect mask is empty");

    uint32_t mip_levels_count = desc.view_mip_levels_count;
    if (mip_levels_count == 0) {
        mip_levels_count = image.mip_levels() - desc.view_base_mip_level;
    }

    logger.check(desc.view_base_mip_level < image.mip_levels(), "Texture view base mip level is out of range");
    logger.check(mip_levels_count <= image.mip_levels() - desc.view_base_mip_level, "Texture view mip level range is out of bounds");

    logger.check(desc.view_base_array_layer == 0, "Texture2D view base array layer must be 0");
    logger.check(desc.view_array_layers_count == 1, "Texture2D view array layer count must be 1");
    logger.check(desc.view_type == VK_IMAGE_VIEW_TYPE_2D, "Texture2D view type must be VK_IMAGE_VIEW_TYPE_2D");

    return VulkanImageView(
        device,
        image,
        desc.view_aspect_mask,
        desc.view_base_mip_level,
        mip_levels_count,
        desc.view_base_array_layer,
        desc.view_array_layers_count,
        desc.view_type
    );
}

VkSamplerCreateInfo VulkanTexture2D::create_sampler_desc(
    const VulkanTexture2DDesc& desc,
    uint32_t mip_levels)
{
    LOG_NAMED("VulkanTexture2D");

    logger.check(mip_levels != 0, "Sampler mip levels count is zero");

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    sampler_info.magFilter = desc.sampler_mag_filter;
    sampler_info.minFilter = desc.sampler_min_filter;

    sampler_info.addressModeU = desc.sampler_address_mode_u;
    sampler_info.addressModeV = desc.sampler_address_mode_v;
    sampler_info.addressModeW = desc.sampler_address_mode_w;

    sampler_info.anisotropyEnable = desc.sampler_anisotropy_enable;
    sampler_info.maxAnisotropy = desc.sampler_max_anisotropy;

    sampler_info.borderColor = desc.sampler_border_color;
    sampler_info.unnormalizedCoordinates = desc.sampler_unnormalized_coordinates;

    sampler_info.compareEnable = desc.sampler_compare_enable;
    sampler_info.compareOp = desc.sampler_compare_op;

    sampler_info.mipmapMode = desc.sampler_mipmap_mode;
    sampler_info.mipLodBias = desc.sampler_mip_lod_bias;
    sampler_info.minLod = desc.sampler_min_lod;

    if (desc.sampler_max_lod < 0.0f) {
        sampler_info.maxLod = static_cast<float>(mip_levels - 1);
    } else {
        sampler_info.maxLod = desc.sampler_max_lod;
    }

    return sampler_info;
}