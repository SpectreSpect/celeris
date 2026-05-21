#include "vulkan_image_view.h"
#include "vulkan_device.h"
#include "vulkan_swapchain.h"
#include "vulkan_image_temp.h"

#include <utility>

VulkanImageView::VulkanImageView(const VkImageViewCreateInfo& create_info, const VulkanDevice& device)
    :   m_device(device.handle())
{
    LOG_METHOD();

    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");

    VkResult result = vkCreateImageView(
        m_device,
        &create_info,
        nullptr,
        &m_image_view
    );

    logger.check(result == VK_SUCCESS, "Failed to create swapchain image view");
}

VulkanImageView::~VulkanImageView() {
    destroy();
}

void VulkanImageView::destroy() {
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

VkImageViewCreateInfo VulkanImageView::create_depth_desc(VkImage image, VkFormat format) {
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

    create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    return create_info;
}

std::vector<VulkanImageView> VulkanImageView::from_depth_images(
    const VulkanDevice& device,
    const std::vector<VulkanImageTemp>& images,
    VkFormat format)
{
    LOG_NAMED("VulkanImageView");

    std::vector<VulkanImageView> image_views;
    image_views.reserve(images.size());

    for (const VulkanImageTemp& image : images) {
        image_views.emplace_back(
            VulkanImageView::create_depth_desc(
                image.handle(),
                format
            ),
            device
        );
    }

    return image_views;
}