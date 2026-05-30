#pragma once

#include <optional>

#include "../vulkan_self/image/vulkan_texture_2d.h"
#include "../vulkan_self/image/cubemap.h"
#include "pbr/equirect_to_cubemap_pass.h"
#include "../path_utils.h"

class VulkanEngine;
class VulkanResourceLoader;
class ComputePassManager;

class TextureManager {
private:
    VulkanEngine& m_engine;
    VulkanResourceLoader& m_resource_loader;

public:
    EquirectToCubemapPass equirect_to_cubemap_pass;

    VulkanTexture2D dirt_texture;
    VulkanTexture2D rock_texture;
    VulkanTexture2D st_peters_square_night_4k_hdr;

    std::optional<Cubemap> dirt_env_map = std::nullopt;
    std::optional<Cubemap> st_peters_square_night_4k_hdr_env_map = std::nullopt;

    TextureManager(VulkanEngine& engine, VulkanResourceLoader& resource_loader, ComputePassManager& compute_pass_manager);

private:
    VulkanTexture2D load_rbga8(const std::filesystem::path& path);
};