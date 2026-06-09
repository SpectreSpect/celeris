#pragma once
#include "video_buffer.h"
#include "descriptor_pool.h"
#include "descriptor_set_layout.h"
#include "descriptor_set.h"
#include "image/cubemap.h"
#include <unordered_map>
#include <variant>
#include <stdexcept>

class Texture2D;
class VulkanImageView;

class DescriptorSetBundle {
public:
    DescriptorSetLayout descriptor_set_layout;
    DescriptorPool descriptor_pool;
    DescriptorSet descriptor_set;

    void bind_uniform_buffer(uint32_t binding, VideoBuffer& buffer);
    void bind_combined_image_sampler(uint32_t binding, Texture2D& texture);
    void bind_combined_image_sampler(uint32_t binding, Cubemap& texture);
    void bind_storage_buffer(uint32_t binding, VideoBuffer& buffer);
    void bind_image_storage(uint32_t binding, Cubemap& texture);
    void bind_image_storage(uint32_t binding, Texture2D& texture);
    void bind_image_storage(uint32_t binding, VulkanImageView& image_view);

    DescriptorSetBundle() = default;
};

class DescriptorSetBundleBuilder {
public:
    struct Entry {
        VkDescriptorType descriptor_type;
        VkShaderStageFlags shader_stage_flags;
        std::variant<std::monostate, VideoBuffer*, Texture2D*, Cubemap*> resource;
    };

    std::unordered_map<uint32_t, Entry> entries;

    DescriptorSetBundleBuilder() = default;

    void add_uniform_buffer(
        uint32_t binding,
        VkShaderStageFlags stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    );

    void add_storage_buffer(
        uint32_t binding,
        VkShaderStageFlags stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    );

    void add_combined_image_sampler(
        uint32_t binding,
        VkShaderStageFlags stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    );

    void add_image_storage(
        uint32_t binding,
        VkShaderStageFlags stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    );

    void add_uniform_buffer(
        uint32_t binding,
        VideoBuffer& video_buffer,
        VkShaderStageFlags stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    );

    void add_storage_buffer(
        uint32_t binding,
        VideoBuffer& video_buffer,
        VkShaderStageFlags stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    );

    void add_combined_image_sampler(
        uint32_t binding,
        Texture2D& texture,
        VkShaderStageFlags stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    );

    void add_combined_image_sampler(
        uint32_t binding,
        Cubemap& texture,
        VkShaderStageFlags stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    );

    void add_image_storage(
        uint32_t binding,
        Cubemap& texture,
        VkShaderStageFlags stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    );

    DescriptorSetBundle create(VkDevice& device);
};