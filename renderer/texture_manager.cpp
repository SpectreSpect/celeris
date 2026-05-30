#include "texture_manager.h"
#include "../vulkan_self/vulkan_engine.h"
#include "../vulkan_self/image/cpu_image.h"
#include "../vulkan_self/vulkan_resource_loader.h"

TextureManager::TextureManager(VulkanEngine& engine, VulkanResourceLoader& resource_loader, ComputePassManager& compute_pass_manager)
    :   m_engine(engine),
        equirect_to_cubemap_pass(engine, compute_pass_manager),
        brdf_lut_pass(engine, compute_pass_manager),
        prefilter_pass(engine, compute_pass_manager),
        irradiance_pass(engine, compute_pass_manager),
        brdf_lut(brdf_lut_pass.generate(512, 512)),
        m_resource_loader(resource_loader),
        dirt_texture(load_rbga8(path_utils::executable_dir() / "assets" / "textures" / "minecraft_dirt" / "texture.png")),
        rock_texture(load_rbga8(path_utils::executable_dir() / "assets" / "textures" / "rock" / "albedo.jpg")),
        st_peters_square_night_4k_hdr(load_rbga8(path_utils::executable_dir() / "assets" / "hdr" / "studio_kominka_02_4k.hdr"))
        {
        // st_peters_square_night_4k_hdr(load_rbga8(path_utils::executable_dir() / "assets" / "textures" / "hdr" / "st_peters_square_night_4k.hdr")){
    resource_loader.submit();

    dirt_env_map.emplace(equirect_to_cubemap_pass.generate(dirt_texture, 100));
    st_peters_square_night_4k_hdr_env_map.emplace(equirect_to_cubemap_pass.generate(st_peters_square_night_4k_hdr, 2048));
    st_peters_square_night_4k_hdr_prefilter_map.emplace(prefilter_pass.generate(*st_peters_square_night_4k_hdr_env_map, 512));
    st_peters_square_night_4k_hdr_irradiance_map.emplace(irradiance_pass.generate(*st_peters_square_night_4k_hdr_env_map, 32));
}

VulkanTexture2D TextureManager::load_rbga8(const std::filesystem::path& path) {
    CpuImage dirt_cpu_image = CpuImage::load_rgba8_image(path);

    VulkanTexture2D texture(m_engine.physical_device(), m_engine.device(), dirt_cpu_image.extent2d());

    m_resource_loader.upload_sampled_texture_2d(dirt_cpu_image, texture);

    return texture;
}