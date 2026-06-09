#include "resource_loader.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"

void ResourceLoader::create(VulkanEngine& engine, VkDeviceSize staging_buffer_size) {
    this->engine = &engine;
    
    compute_queue_family_id = vulkan_utils::find_compute_queue_family(engine.physicalDevice);
    vkGetDeviceQueue(engine.device, compute_queue_family_id, 0, &compute_queue);

    command_pool.create(engine.device, engine.physicalDevice, compute_queue_family_id, compute_queue);
    command_buffer.create(command_pool);

    staging_buffer.create(engine, staging_buffer_size);
    fence.create(engine.device);
}

Texture2D ResourceLoader::load_hdr_texture2d(const std::filesystem::path& filepath, uint32_t mip_levels, VkImageUsageFlags usage) {
    if (!engine) {
        std::string message = "engine was null";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }
    
    stbi_set_flip_vertically_on_load(1);

    int w = 0;
    int h = 0;
    int channels = 0;

    float* pixels = stbi_loadf(filepath.string().c_str(), &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels) {
        std::string message = "failed to load texture";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    Texture2D texture;

    try {
        texture.create(*engine, w, h, mip_levels, usage, VK_FORMAT_R32G32B32A32_SFLOAT); //VK_FORMAT_R32G32B32A32_SFLOAT

        const VkDeviceSize image_size = static_cast<VkDeviceSize>(w) * h * 4 * sizeof(float);

        if (image_size > staging_buffer.size_bytes) {
            std::string message = "staging buffer is too small for the image data";
            std::cout << message << std::endl;
            throw std::runtime_error(message);
        }


        texture.upload_data(pixels, image_size, command_buffer, staging_buffer);

        command_buffer.submit_and_wait(compute_queue, fence);
    } catch (...) {
        stbi_image_free(pixels);
        std::string message = "failed to create Texture2D";
        throw std::runtime_error(message);
    }

    stbi_image_free(pixels);

    return texture;
}

Texture2D ResourceLoader::load_texture2d(const std::filesystem::path& filepath,
                                         uint32_t mip_levels,
                                         VkImageUsageFlags usage) {
    if (!engine) {
        std::string message = "engine was null";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    stbi_set_flip_vertically_on_load(1);

    int w = 0;
    int h = 0;
    int channels = 0;

    stbi_uc* pixels = stbi_load(filepath.string().c_str(), &w, &h, &channels, STBI_rgb_alpha);
    if (!pixels) {
        std::string message = "failed to load texture";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    Texture2D texture;
    
    try {
        texture.create(*engine, w, h, mip_levels, usage, VK_FORMAT_R8G8B8A8_SRGB);

        const VkDeviceSize image_size =
            static_cast<VkDeviceSize>(w) * h * 4 * sizeof(stbi_uc);

        if (image_size > staging_buffer.size_bytes) {
            std::string message = "staging buffer is too small for the image data";
            std::cout << message << std::endl;
            throw std::runtime_error(message);
        }

        texture.upload_data(pixels, image_size, command_buffer, staging_buffer);

        command_buffer.submit_and_wait(compute_queue, fence);
    } catch (...) {
        stbi_image_free(pixels);
        throw std::runtime_error("failed to create Texture2D");
    }

    stbi_image_free(pixels);

    return texture;
}