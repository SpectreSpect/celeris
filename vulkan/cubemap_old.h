#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <string>
#include <cstdint>

class VulkanEngine;

class Cubemap {
public:
    enum class MinFilter {
        Nearest,
        Linear,
        NearestMipmapNearest,
        LinearMipmapNearest,
        NearestMipmapLinear,
        LinearMipmapLinear
    };

    enum class MagFilter {
        Nearest,
        Linear
    };

    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;

    VkDevice* device = nullptr;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;

    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    uint32_t size = 0;
    uint32_t mip_levels = 1;

    Cubemap() = default;

    // Face order:
    // 0 = +X
    // 1 = -X
    // 2 = +Y
    // 3 = -Y
    // 4 = +Z
    // 5 = -Z
    Cubemap(VulkanEngine& engine,
            const std::array<std::string, 6>& facepaths,
            MagFilter mag_filter = MagFilter::Linear,
            MinFilter min_filter = MinFilter::Linear,
            bool srgb = false,
            bool flipY = false);

    ~Cubemap();

    Cubemap(const Cubemap&) = delete;
    Cubemap& operator=(const Cubemap&) = delete;

    Cubemap(Cubemap&& other) noexcept;
    Cubemap& operator=(Cubemap&& other) noexcept;

    bool loadFromFaces(VulkanEngine& engine,
                       const std::array<std::string, 6>& facepaths,
                       MagFilter mag_filter = MagFilter::Linear,
                       MinFilter min_filter = MinFilter::Linear,
                       bool srgb = false,
                       bool flipY = false);

    void createEmpty(VulkanEngine& engine,
                     int face_size,
                     VkFormat image_format = VK_FORMAT_R16G16B16A16_SFLOAT,
                     MagFilter mag_filter = MagFilter::Linear,
                     MinFilter min_filter = MinFilter::Linear);

    void destroy();

    VkDescriptorImageInfo descriptor_info() const;

    int faceSize() const { return static_cast<int>(size); }
    int mipLevels() const { return static_cast<int>(mip_levels); }

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

    static uint32_t compute_mip_levels(uint32_t size);
    static VkFilter to_vk_mag_filter(MagFilter filter);
    static MinFilterInfo to_vk_min_filter(MinFilter filter);

    void create_image(VulkanEngine& engine, VkFormat image_format, VkImageUsageFlags usage);
    void create_image_view();
    void create_sampler(MagFilter mag_filter, MinFilter min_filter);



    void copy_buffer_to_cubemap(VulkanEngine& engine, VkBuffer buffer);
    void generate_mipmaps(VulkanEngine& engine);

    VkCommandBuffer begin_single_time_commands(VulkanEngine& engine);
    void end_single_time_commands(VulkanEngine& engine, VkCommandBuffer cmd);
};