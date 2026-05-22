#include "vulkan_image_view.h"

#include <utility>

#include "../vulkan_device.h"
#include "../vulkan_swapchain.h"
#include "vulkan_image.h"

VulkanImageView::VulkanImageView(
    const VkImageViewCreateInfo& create_info, 
    const VulkanDevice& device)
{
    LOG_METHOD();
    create_view(create_info, device);
}


VulkanImageView::VulkanImageView(
    const VulkanDevice& device,
    const VulkanImage& image,
    VkImageAspectFlags aspect_mask,
    uint32_t base_mip_level,
    uint32_t mip_levels_count,
    uint32_t base_array_level,
    uint32_t array_levels_count) 
{
    LOG_METHOD();

    VkImageViewCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = image.handle();
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format = image.format();

    create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    create_info.subresourceRange.aspectMask = aspect_mask;
    create_info.subresourceRange.baseMipLevel = base_mip_level;
    create_info.subresourceRange.levelCount = mip_levels_count;
    create_info.subresourceRange.baseArrayLayer = base_array_level;
    create_info.subresourceRange.layerCount = array_levels_count;

    create_view(create_info, device);
}

VulkanImageView::~VulkanImageView() noexcept {
    destroy();
}

void VulkanImageView::destroy() noexcept {
    if (m_device != VK_NULL_HANDLE && m_image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_image_view, nullptr);
    }

    m_image_view = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
}

VulkanImageView::VulkanImageView(VulkanImageView&& other) noexcept
    :   m_image_view(std::exchange(other.m_image_view, VK_NULL_HANDLE)),
        m_device(std::exchange(other.m_device, VK_NULL_HANDLE)) {}

VulkanImageView& VulkanImageView::operator=(VulkanImageView&& other) noexcept {
    if (this != &other) {
        destroy();

        m_image_view = std::exchange(other.m_image_view, VK_NULL_HANDLE);
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    }

    return *this;
}

VkImageView VulkanImageView::handle() const noexcept {
    return m_image_view;
}

VkImageViewCreateInfo VulkanImageView::create_swapchain_desc(VkImage image, VkFormat format) {
    LOG_NAMED("ImageView");

    VkImageViewCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = image;
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format = format;

    create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    return create_info;
}

std::vector<VulkanImageView> VulkanImageView::from_swapchain(const VulkanDevice& device, const VulkanSwapchain& swapchain) {
    LOG_NAMED("VulkanImageView");

    std::vector<VulkanImageView> image_views;
    image_views.reserve(swapchain.images().size());

    for (size_t i = 0; i < swapchain.images().size(); i++) {
        image_views.emplace_back(
            VulkanImageView::create_swapchain_desc(
                swapchain.image(i), 
                swapchain.image_format()
            ),
            device
        );
    }

    return image_views;
}

void VulkanImageView::create_view(const VkImageViewCreateInfo& desc, const VulkanDevice& device) {
    LOG_METHOD();

    logger.check(m_image_view == VK_NULL_HANDLE, "Image view was already initialized");
    logger.check(device.handle() != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(desc.sType == VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, "Invalid VkImageViewCreateInfo::sType");
    logger.check(desc.image != VK_NULL_HANDLE, "Image is not initialized");
    logger.check(desc.format != VK_FORMAT_UNDEFINED, "Image view format is undefined");
    logger.check(desc.subresourceRange.aspectMask != 0, "Image view aspect mask is empty");

    m_device = device.handle();

    VkResult result = vkCreateImageView(
        m_device,
        &desc,
        nullptr,
        &m_image_view
    );

    logger.check(result == VK_SUCCESS, "Failed to create swapchain image view");
}
