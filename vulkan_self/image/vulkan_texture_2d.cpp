#include "vulkan_texture_2d.h"

#include "../vulkan_physical_device.h"
#include "../vulkan_device.h"

#include <utility>

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
    VkExtent2D extent2d)
    :   m_image(
            physical_device,
            device,
            extent2d,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        ),
        m_view(device, m_image),
        m_sampler(device),
        m_layout(VK_IMAGE_LAYOUT_UNDEFINED) {}

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
