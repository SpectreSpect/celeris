#pragma once
#include "../vulkan_engine.h"
#include "command_pool.h"
#include "command_buffer.h"
#include "image/texture2d.h"
#include <filesystem>

class ResourceLoader {
public:
    VulkanEngine* engine = nullptr;
    CommandPool command_pool;
    CommandBuffer command_buffer;
    VideoBuffer staging_buffer;
    Fence fence;
    
    uint32_t compute_queue_family_id;
    VkQueue compute_queue;

    ResourceLoader() = default;
    void create(VulkanEngine& engine, VkDeviceSize staging_buffer_size);

    Texture2D load_hdr_texture2d(const std::filesystem::path& filepath, uint32_t mip_levels, VkImageUsageFlags usage);
    Texture2D load_texture2d(const std::filesystem::path& filepath, uint32_t mip_levels, VkImageUsageFlags usage);

    
};