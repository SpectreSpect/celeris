#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "../vulkan_self/image/vulkan_texture_2d.h"
#include "../vulkan_self/image/cubemap.h"
#include "../vulkan_self/image/cubemap_array.h"
#include "../renderer/pbr/equirect_to_cubemap_pass.h"
#include "../renderer/pbr/brdf_lut_pass.h"
#include "../renderer/pbr/prefilter_map_pass.h"
#include "../renderer/pbr/irradiance_map_pass.h"
#include "../path_utils.h"

class VulkanEngine;
class VulkanResourceLoader;
class ComputePassManager;

class TextureManager {
private:
    VulkanEngine& m_engine;
    VulkanResourceLoader& m_resource_loader;

public:
    static constexpr uint32_t studio_kominka_02_4k_pbr_map_id = 0;
    static constexpr uint32_t st_peters_square_night_4k_pbr_map_id = 1;
    static constexpr uint32_t ferndale_studio_06_4k_pbr_map_id = 2;
    static constexpr uint32_t qwantani_moonrise_puresky_4k_pbr_map_id = 3;
    static constexpr uint32_t citrus_orchard_puresky_4k_pbr_map_id = 4;
    static constexpr uint32_t moonless_golf_4k_pbr_map_id = 5;

    static constexpr uint32_t max_pbr_cubemap_count = 6;

    EquirectToCubemapPass equirect_to_cubemap_pass;
    BrdfLutPass brdf_lut_pass;
    PrefilterPass prefilter_pass;
    IrradiancePass irradiance_pass;

    VulkanTexture2D brdf_lut;
    CubemapArray irradiance_maps;
    CubemapArray prefilter_maps;

    VulkanTexture2D dirt_texture;
    VulkanTexture2D rock_texture;
    std::vector<VulkanTexture2D> hdr_textures;
    std::vector<Cubemap> hdr_env_maps;

    std::optional<Cubemap> dirt_env_map = std::nullopt;

    TextureManager(VulkanEngine& engine, VulkanResourceLoader& resource_loader, ComputePassManager& compute_pass_manager);

    void generate_pbr_maps(Cubemap& environment_map, uint32_t pbr_map_id);

private:
    VulkanTexture2D load_rbga8(const std::filesystem::path& path);
};
