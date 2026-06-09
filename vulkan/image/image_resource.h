#pragma once
#include <vector>
#include <cstdint>
#include <vulkan/vulkan.h>

#include "image_transition_state.h"

class VulkanEngine;
class CommandBuffer;

class ImageResource {
public:
    struct ImageCreateDesc {
        VkImageType image_type = VK_IMAGE_TYPE_2D;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkExtent3D extent{1, 1, 1};

        uint32_t mip_levels = 1;
        uint32_t array_layers = 1;

        VkImageUsageFlags usage = 0;
        VkImageCreateFlags flags = 0;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;

        VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
        std::vector<uint32_t> queue_family_indices;

        VkMemoryPropertyFlags memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    };

    VulkanEngine* engine = nullptr;

    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

    VkImageType image_type = VK_IMAGE_TYPE_2D;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent3D extent{1, 1, 1};
    uint32_t mip_levels = 1;
    uint32_t array_layers = 1;
    VkImageUsageFlags usage = 0;
    VkImageCreateFlags flags = 0;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

    VkImageAspectFlags aspect = 0;
    VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;

    ImageTransitionState transition_state;

    ImageResource() = default;
    ImageResource(VulkanEngine* engine, const ImageCreateDesc& desc);
    
    ~ImageResource();

    void destroy();

    ImageResource(const ImageResource&) = delete;
    ImageResource& operator=(const ImageResource&) = delete;
    
    ImageResource(ImageResource&& other) noexcept;
    ImageResource& operator=(ImageResource&& other) noexcept;

    void transition_image(
        CommandBuffer& command_buffer,
        ImageTransitionState::ImageUsage new_usage,
        uint32_t base_mip_level,
        uint32_t level_count,
        uint32_t base_array_layer,
        uint32_t layer_count,
        bool discard_contents = false
    );

    void transition_image_all(
        CommandBuffer& command_buffer,
        ImageTransitionState::ImageUsage new_usage,
        bool discard_contents = false
    );

    void transition_image_mip(
        CommandBuffer& command_buffer,
        uint32_t mip_level,
        ImageTransitionState::ImageUsage new_usage,
        bool discard_contents = false
    );

    // Assumes mip 0 already contains valid data.
    // Assumes the image was created with TRANSFER_SRC and TRANSFER_DST usage.
    void generate_mipmaps(CommandBuffer& command_buffer);

    static ImageCreateDesc texture2d_desc(
        VkFormat format,
        uint32_t width,
        uint32_t height,
        uint32_t mip_levels,
        VkImageUsageFlags usage
    );

    static ImageCreateDesc cubemap_desc(
        VkFormat format,
        uint32_t face_size,
        uint32_t mip_levels,
        VkImageUsageFlags usage
    );

    static uint32_t calc_mip_levels(uint32_t width, uint32_t height);
};
