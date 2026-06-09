#pragma once
#include <stdexcept>
#include <utility>

#include "image_resource.h"
#include "image_view.h"
#include "sampler.h"
#include "../video_buffer.h"

class CommandBuffer;

class Cubemap {
public:
    VulkanEngine* engine = nullptr;
    ImageResource image_resource;
    VulkanImageView image_view;
    Sampler sampler;
    uint32_t face_size = 0;

    Cubemap() = default;
    Cubemap(
        VulkanEngine* engine,
        uint32_t face_size,
        uint32_t mip_levels,
        VkImageUsageFlags usage,
        VkFormat format
    );

    Cubemap(const Cubemap&) = default;
    Cubemap& operator=(const Cubemap&) = default;
    
    Cubemap(Cubemap&&) noexcept = default;
    Cubemap& operator=(Cubemap&&) noexcept = default;

    static uint32_t calc_mip_levels(uint32_t face_size);
};
