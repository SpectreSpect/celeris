#pragma once
#include <utility>
#include <stdexcept>

#include "image_resource.h"
#include "../vulkan_utils.h"

class VulkanEngine;

class VulkanImageView {
public:
    struct CreateDesc {
        ImageResource* image = nullptr;
        VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkImageSubresourceRange subresource_range{};
        VkComponentMapping components{};
        VkImageViewCreateFlags flags = 0;
    };

    VulkanEngine* engine = nullptr;
    ImageResource* image = nullptr;
    VkImageView view = VK_NULL_HANDLE;

    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_2D;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageSubresourceRange subresource_range{};
    VkComponentMapping components{};
    VkImageViewCreateFlags flags = 0;

    VulkanImageView() = default;
    VulkanImageView(VulkanEngine* engine, const CreateDesc& desc);

    ~VulkanImageView();
    void destroy();

    VulkanImageView(const VulkanImageView&) = delete;
    VulkanImageView& operator=(const VulkanImageView&) = delete;

    VulkanImageView(VulkanImageView&& other) noexcept;
    VulkanImageView& operator=(VulkanImageView&& other) noexcept;

    static CreateDesc texture2d_desc(ImageResource* image);
    static CreateDesc cubemap_desc(ImageResource* image);
    static CreateDesc cubemap_mip_desc(ImageResource* image, uint32_t mip_level);
};