#pragma once
#include <utility>
#include <stdexcept>

#include "sampler.h"
#include "image_view.h"
#include "image_resource.h"
#include "../video_buffer.h"

class CommandBuffer;

class Texture2D {
public:
    VulkanEngine* engine = nullptr;

    ImageResource image_resource;
    VulkanImageView image_view;
    Sampler sampler;

    Texture2D() = default;
    Texture2D(
        VulkanEngine* engine,
        uint32_t width,
        uint32_t height,
        uint32_t mip_levels,
        VkImageUsageFlags usage,
        VkFormat format
    );

    ~Texture2D() = default;

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    Texture2D(Texture2D&& other) noexcept = default;
    Texture2D& operator=(Texture2D&& other) noexcept = default;

    void record_full_upload(
        CommandBuffer& command_buffer,
        VideoBuffer& staging_buffer
    );

    void upload_data(
        CommandBuffer& cmd,
        VideoBuffer& staging,
        Fence& fence,
        const void* data,
        size_t size_bytes
    );

    void read_data(CommandBuffer& command_buffer, VideoBuffer& staging_buffer);

    void generate_mipmaps(CommandBuffer& command_buffer);

    static uint32_t calc_mip_levels(uint32_t width, uint32_t height);
};