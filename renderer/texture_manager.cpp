#include "texture_manager.h"
#include "../vulkan_self/vulkan_engine.h"
#include "../vulkan_self/image/cpu_image.h"
#include "../vulkan_self/vulkan_resource_loader.h"

TextureManager::TextureManager(VulkanEngine& engine, VulkanResourceLoader& resource_loader)
    :   m_engine(engine),
        m_resource_loader(resource_loader),
        dirt_texture(load_rbga8(path_utils::executable_dir() / "assets" / "textures" / "minecraft_dirt" / "texture.png")),
        rock_texture(load_rbga8(path_utils::executable_dir() / "assets" / "textures" / "rock" / "albedo.jpg")){
        // st_peters_square_night_4k_hdr(load_rbga8(path_utils::executable_dir() / "assets" / "textures" / "hdr" / "st_peters_square_night_4k.hdr")){
    resource_loader.submit();
}

VulkanTexture2D TextureManager::load_rbga8(const std::filesystem::path& path) {
    CpuImage dirt_cpu_image = CpuImage::load_rgba8_image(path);

    VulkanTexture2D texture(m_engine.physical_device(), m_engine.device(), dirt_cpu_image.extent2d());

    m_resource_loader.upload_sampled_texture_2d(dirt_cpu_image, texture);

    return texture;
}