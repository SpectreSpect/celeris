#include "render_target_2d.h"

#include <array>
#include <stdexcept>

#include "../vulkan_engine.h"
#include "render_pass.h"
#include "vulkan_utils.h"

RenderTarget2D::~RenderTarget2D() {
    destroy();
}

RenderTarget2D::RenderTarget2D(RenderTarget2D&& other) noexcept {
    color_image = other.color_image;
    color_memory = other.color_memory;
    color_view = other.color_view;

    depth_image = other.depth_image;
    depth_memory = other.depth_memory;
    depth_view = other.depth_view;

    framebuffer = other.framebuffer;

    device = other.device;
    color_format = other.color_format;
    depth_format = other.depth_format;
    width = other.width;
    height = other.height;
    has_depth = other.has_depth;

    other.color_image = VK_NULL_HANDLE;
    other.color_memory = VK_NULL_HANDLE;
    other.color_view = VK_NULL_HANDLE;

    other.depth_image = VK_NULL_HANDLE;
    other.depth_memory = VK_NULL_HANDLE;
    other.depth_view = VK_NULL_HANDLE;

    other.framebuffer = VK_NULL_HANDLE;

    other.device = nullptr;
    other.color_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    other.depth_format = VK_FORMAT_UNDEFINED;
    other.width = 0;
    other.height = 0;
    other.has_depth = false;
}

RenderTarget2D& RenderTarget2D::operator=(RenderTarget2D&& other) noexcept {
    if (this != &other) {
        destroy();

        color_image = other.color_image;
        color_memory = other.color_memory;
        color_view = other.color_view;

        depth_image = other.depth_image;
        depth_memory = other.depth_memory;
        depth_view = other.depth_view;

        framebuffer = other.framebuffer;

        device = other.device;
        color_format = other.color_format;
        depth_format = other.depth_format;
        width = other.width;
        height = other.height;
        has_depth = other.has_depth;

        other.color_image = VK_NULL_HANDLE;
        other.color_memory = VK_NULL_HANDLE;
        other.color_view = VK_NULL_HANDLE;

        other.depth_image = VK_NULL_HANDLE;
        other.depth_memory = VK_NULL_HANDLE;
        other.depth_view = VK_NULL_HANDLE;

        other.framebuffer = VK_NULL_HANDLE;

        other.device = nullptr;
        other.color_format = VK_FORMAT_R16G16B16A16_SFLOAT;
        other.depth_format = VK_FORMAT_UNDEFINED;
        other.width = 0;
        other.height = 0;
        other.has_depth = false;
    }
    return *this;
}

void RenderTarget2D::destroy() {
    if (device && framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(*device, framebuffer, nullptr);
        framebuffer = VK_NULL_HANDLE;
    }

    if (device && depth_view != VK_NULL_HANDLE) {
        vkDestroyImageView(*device, depth_view, nullptr);
        depth_view = VK_NULL_HANDLE;
    }

    if (device && depth_image != VK_NULL_HANDLE) {
        vkDestroyImage(*device, depth_image, nullptr);
        depth_image = VK_NULL_HANDLE;
    }

    if (device && depth_memory != VK_NULL_HANDLE) {
        vkFreeMemory(*device, depth_memory, nullptr);
        depth_memory = VK_NULL_HANDLE;
    }

    if (device && color_view != VK_NULL_HANDLE) {
        vkDestroyImageView(*device, color_view, nullptr);
        color_view = VK_NULL_HANDLE;
    }

    if (device && color_image != VK_NULL_HANDLE) {
        vkDestroyImage(*device, color_image, nullptr);
        color_image = VK_NULL_HANDLE;
    }

    if (device && color_memory != VK_NULL_HANDLE) {
        vkFreeMemory(*device, color_memory, nullptr);
        color_memory = VK_NULL_HANDLE;
    }

    device = nullptr;
    color_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    depth_format = VK_FORMAT_UNDEFINED;
    width = 0;
    height = 0;
    has_depth = false;
}

void RenderTarget2D::create_color_only(
    VulkanEngine& engine,
    RenderPass& render_pass,
    uint32_t width,
    uint32_t height,
    VkFormat color_format,
    VkImageUsageFlags extra_color_usage
) {
    create_internal(
        engine,
        render_pass,
        width,
        height,
        color_format,
        false,
        VK_FORMAT_UNDEFINED,
        extra_color_usage
    );
}

void RenderTarget2D::create_color_and_depth(
    VulkanEngine& engine,
    RenderPass& render_pass,
    uint32_t width,
    uint32_t height,
    VkFormat color_format,
    VkFormat depth_format,
    VkImageUsageFlags extra_color_usage
) {
    create_internal(
        engine,
        render_pass,
        width,
        height,
        color_format,
        true,
        depth_format,
        extra_color_usage
    );
}

void RenderTarget2D::create_internal(
    VulkanEngine& engine,
    RenderPass& render_pass,
    uint32_t width,
    uint32_t height,
    VkFormat color_format,
    bool create_depth,
    VkFormat depth_format,
    VkImageUsageFlags extra_color_usage
) {
    destroy();

    if (width == 0 || height == 0) {
        throw std::runtime_error("RenderTarget2D dimensions must be non-zero");
    }

    if (render_pass.render_pass == VK_NULL_HANDLE) {
        throw std::runtime_error("RenderTarget2D: render_pass was VK_NULL_HANDLE");
    }

    this->device = &engine.device;
    this->width = width;
    this->height = height;
    this->color_format = color_format;
    this->has_depth = create_depth;
    this->depth_format = create_depth
        ? (depth_format == VK_FORMAT_UNDEFINED
            ? VulkanEngine::findDepthFormat(engine.physicalDevice)
            : depth_format)
        : VK_FORMAT_UNDEFINED;

    VkImageUsageFlags color_usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        extra_color_usage;

    VulkanEngine::createImage(
        engine.device,
        engine.physicalDevice,
        width,
        height,
        this->color_format,
        VK_IMAGE_TILING_OPTIMAL,
        color_usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        color_image,
        color_memory
    );

    color_view = VulkanEngine::createImageView(
        engine.device,
        color_image,
        this->color_format,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    if (this->has_depth) {
        VulkanEngine::createImage(
            engine.device,
            engine.physicalDevice,
            width,
            height,
            this->depth_format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            depth_image,
            depth_memory
        );

        const VkImageAspectFlags depth_aspect =
            VulkanEngine::hasStencilComponent(this->depth_format)
                ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
                : VK_IMAGE_ASPECT_DEPTH_BIT;

        depth_view = VulkanEngine::createImageView(
            engine.device,
            depth_image,
            this->depth_format,
            depth_aspect
        );
    }

    if (this->has_depth) {
        std::array<VkImageView, 2> attachments = {
            color_view,
            depth_view
        };

        VkFramebufferCreateInfo fb_info{};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.renderPass = render_pass.render_pass;
        fb_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        fb_info.pAttachments = attachments.data();
        fb_info.width = width;
        fb_info.height = height;
        fb_info.layers = 1;

        vulkan_utils::vk_check(
            vkCreateFramebuffer(engine.device, &fb_info, nullptr, &framebuffer),
            "vkCreateFramebuffer(RenderTarget2D color+depth)"
        );
    } else {
        std::array<VkImageView, 1> attachments = {
            color_view
        };

        VkFramebufferCreateInfo fb_info{};
        fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fb_info.renderPass = render_pass.render_pass;
        fb_info.attachmentCount = static_cast<uint32_t>(attachments.size());
        fb_info.pAttachments = attachments.data();
        fb_info.width = width;
        fb_info.height = height;
        fb_info.layers = 1;

        vulkan_utils::vk_check(
            vkCreateFramebuffer(engine.device, &fb_info, nullptr, &framebuffer),
            "vkCreateFramebuffer(RenderTarget2D color-only)"
        );
    }
}