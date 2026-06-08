#include "texture_manager.h"
#include "../vulkan_self/vulkan_engine.h"
#include "../vulkan_self/image/cpu_image.h"
#include "../vulkan_self/vulkan_resource_loader.h"

#include <array>

namespace {
constexpr std::array<const char*, TextureManager::max_pbr_cubemap_count> pbr_hdr_filenames = {
    // "studio_kominka_02_4k.hdr"
    // "st_peters_square_night_4k.hdr"
    // "ferndale_studio_06_4k.hdr",
    "qwantani_moonrise_puresky_4k.hdr",
    // "citrus_orchard_puresky_4k.hdr",
    // "moonless_golf_4k.hdr",
    // "mud_road_puresky_4k.hdr",
    // "snowy_field_4k.hdr"
};
}

TextureManager::TextureManager(VulkanEngine& engine, VulkanResourceLoader& resource_loader, ComputePassManager& compute_pass_manager)
    :   m_engine(engine),
        equirect_to_cubemap_pass(engine, compute_pass_manager),
        brdf_lut_pass(engine, compute_pass_manager),
        prefilter_pass(engine, compute_pass_manager),
        irradiance_pass(engine, compute_pass_manager),
        brdf_lut(brdf_lut_pass.generate(512, 512)),
        irradiance_maps(
            engine.physical_device(),
            engine.device(),
            VkExtent2D{32, 32},
            VK_FORMAT_R32G32B32A32_SFLOAT,
            max_pbr_cubemap_count,
            CubemapArray::StorageImageUsage::Enabled,
            1
        ),
        prefilter_maps(
            engine.physical_device(),
            engine.device(),
            VkExtent2D{512, 512},
            VK_FORMAT_R32G32B32A32_SFLOAT,
            max_pbr_cubemap_count,
            CubemapArray::StorageImageUsage::Enabled
        ),
        m_resource_loader(resource_loader),
        dirt_texture(load_rbga8(path_utils::executable_dir() / "assets" / "textures" / "minecraft_dirt" / "texture.png")),
        rock_texture(load_rbga8(path_utils::executable_dir() / "assets" / "textures" / "rock" / "albedo.jpg"))
{
    hdr_textures.reserve(pbr_hdr_filenames.size());
    for (const char* filename : pbr_hdr_filenames) {
        hdr_textures.emplace_back(load_rgba32f(path_utils::executable_dir() / "assets" / "hdr" / filename));
    }

    resource_loader.submit();

    hdr_env_maps.reserve(hdr_textures.size());
    for (uint32_t pbr_map_id = 0; pbr_map_id < hdr_textures.size(); ++pbr_map_id) {
        hdr_env_maps.emplace_back(equirect_to_cubemap_pass.generate(hdr_textures[pbr_map_id], 1024));
        generate_pbr_maps(hdr_env_maps.back(), pbr_map_id);
    }
}

void TextureManager::generate_pbr_maps(Cubemap& environment_map, uint32_t pbr_map_id) {
    irradiance_pass.generate_into(
        environment_map,
        irradiance_maps,
        pbr_map_id
    );

    prefilter_pass.generate_into(
        environment_map,
        prefilter_maps,
        pbr_map_id
    );
}

VulkanTexture2D TextureManager::load_rbga8(const std::filesystem::path& path) {
    CpuImage dirt_cpu_image = CpuImage::load_rgba8_image(path);

    VulkanTexture2D texture(m_engine.physical_device(), m_engine.device(), dirt_cpu_image.extent2d());

    m_resource_loader.upload_sampled_texture_2d(dirt_cpu_image, texture);
    m_resource_loader.submit();

    return texture;
}

VulkanTexture2D TextureManager::load_rgba32f(const std::filesystem::path& path) {
    CpuImage cpu_image = CpuImage::load_rgba32f_image(path);

    VulkanTexture2D texture(
        m_engine.physical_device(),
        m_engine.device(),
        cpu_image.extent2d(),
        cpu_image.format()
    );

    m_resource_loader.upload_sampled_texture_2d(cpu_image, texture);
    m_resource_loader.submit();

    return texture;
}
