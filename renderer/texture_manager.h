#pragma once

#include "../vulkan_self/image/vulkan_texture_2d.h"
#include "../path_utils.h"

class VulkanEngine;
class VulkanResourceLoader;

class TextureManager {
private:
    VulkanEngine& m_engine;
    VulkanResourceLoader& m_resource_loader;

public:
    VulkanTexture2D dirt_texture;
    VulkanTexture2D rock_texture;
    // VulkanTexture2D st_peters_square_night_4k_hdr;

    TextureManager(VulkanEngine& engine, VulkanResourceLoader& resource_loader);

private:
    VulkanTexture2D load_rbga8(const std::filesystem::path& path);
};