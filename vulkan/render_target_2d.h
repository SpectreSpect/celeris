#pragma once

#include <vulkan/vulkan.h>

class VulkanEngine;
class RenderPass;

class RenderTarget2D {
public:
    VkImage color_image = VK_NULL_HANDLE;
    VkDeviceMemory color_memory = VK_NULL_HANDLE;
    VkImageView color_view = VK_NULL_HANDLE;

    VkImage depth_image = VK_NULL_HANDLE;
    VkDeviceMemory depth_memory = VK_NULL_HANDLE;
    VkImageView depth_view = VK_NULL_HANDLE;

    VkFramebuffer framebuffer = VK_NULL_HANDLE;

    VkDevice* device = nullptr;

    VkFormat color_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkFormat depth_format = VK_FORMAT_UNDEFINED;

    uint32_t width = 0;
    uint32_t height = 0;

    bool has_depth = false;

    RenderTarget2D() = default;
    ~RenderTarget2D();

    RenderTarget2D(const RenderTarget2D&) = delete;
    RenderTarget2D& operator=(const RenderTarget2D&) = delete;

    RenderTarget2D(RenderTarget2D&& other) noexcept;
    RenderTarget2D& operator=(RenderTarget2D&& other) noexcept;

    void create_color_only(
        VulkanEngine& engine,
        RenderPass& render_pass,
        uint32_t width,
        uint32_t height,
        VkFormat color_format = VK_FORMAT_R16G16B16A16_SFLOAT,
        VkImageUsageFlags extra_color_usage = 0
    );

    void create_color_and_depth(
        VulkanEngine& engine,
        RenderPass& render_pass,
        uint32_t width,
        uint32_t height,
        VkFormat color_format = VK_FORMAT_R16G16B16A16_SFLOAT,
        VkFormat depth_format = VK_FORMAT_UNDEFINED,
        VkImageUsageFlags extra_color_usage = 0
    );

    void destroy();

private:
    void create_internal(
        VulkanEngine& engine,
        RenderPass& render_pass,
        uint32_t width,
        uint32_t height,
        VkFormat color_format,
        bool create_depth,
        VkFormat depth_format,
        VkImageUsageFlags extra_color_usage
    );
};