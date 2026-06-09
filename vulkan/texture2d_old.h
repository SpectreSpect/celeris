#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <cstdint>

class VulkanEngine;

class Texture2D {
public:
    enum class Wrap {
        ClampToEdge,
        Repeat,
        MirroredRepeat
    };

    enum class MagFilter {
        Nearest,
        Linear
    };

    enum class MinFilter {
        Nearest,
        Linear,
        NearestMipmapNearest,
        LinearMipmapNearest,
        NearestMipmapLinear,
        LinearMipmapLinear
    };

    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;

    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkDevice* device = nullptr;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;

    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t mip_levels = 1;

    Texture2D() = default;

    Texture2D(VulkanEngine& engine,
            int width, int height, const void* initial_data,
            Wrap wrap = Wrap::Repeat,
            MagFilter mag_filter = MagFilter::Linear,
            MinFilter min_filter = MinFilter::LinearMipmapLinear,
            bool srgb = false);

    Texture2D(VulkanEngine& engine,
            const std::string& filepath,
            Wrap wrap = Wrap::Repeat,
            MagFilter mag_filter = MagFilter::Linear,
            MinFilter min_filter = MinFilter::LinearMipmapLinear,
            bool srgb = false,
            bool flipY = true);

    ~Texture2D();

    void destroy();

    void create(VulkanEngine& engine,
                int width, int height, const void* initial_data,
                Wrap wrap = Wrap::Repeat,
                MagFilter mag_filter = MagFilter::Linear,
                MinFilter min_filter = MinFilter::LinearMipmapLinear,
                bool srgb = false);

    bool loadFromFile(VulkanEngine& engine,
                      const std::string& filepath,
                      Wrap wrap = Wrap::Repeat,
                      MagFilter mag_filter = MagFilter::Linear,
                      MinFilter min_filter = MinFilter::LinearMipmapLinear,
                      bool srgb = false,
                      bool flipY = true);

    VkDescriptorImageInfo descriptor_info() const;

    void transition_image_layout(VulkanEngine& engine,
                                VkImageLayout new_layout,
                                uint32_t base_mip_level,
                                uint32_t level_count);
    
    void transition_to_general_layout(VulkanEngine& engine);
    void transition_to_shader_read_only_layout(VulkanEngine& engine);

private:
    struct MinFilterInfo {
        VkFilter min_filter;
        VkSamplerMipmapMode mipmap_mode;
        bool needs_mipmaps;
    };

    static VkSamplerAddressMode to_vk_wrap(Wrap wrap);
    static VkFilter to_vk_mag_filter(MagFilter filter);
    static MinFilterInfo to_vk_min_filter(MinFilter filter);
    static uint32_t compute_mip_levels(uint32_t w, uint32_t h);

    void create_image(VulkanEngine& engine, VkFormat image_format, VkImageUsageFlags usage);
    void create_image_view();
    void create_sampler(Wrap wrap, MagFilter mag_filter, MinFilter min_filter);



    void copy_buffer_to_image(VulkanEngine& engine, VkBuffer buffer);
    void generate_mipmaps(VulkanEngine& engine);

    VkCommandBuffer begin_single_time_commands(VulkanEngine& engine);
    void end_single_time_commands(VulkanEngine& engine, VkCommandBuffer cmd);
};