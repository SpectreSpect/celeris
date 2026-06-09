#pragma once
#include <vulkan/vulkan.h>
#include "vulkan_utils.h"
#include "descriptor_set_layout.h"
#include "descriptor_pool.h"
#include "video_buffer.h"
#include "image/cubemap.h"

class Texture2D;
class VulkanImageView;

class DescriptorSet {
public:
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
    VkDevice* device = nullptr;

    DescriptorSet() = default;

    void create(VkDevice& device, DescriptorSetLayout& layout, DescriptorPool& pool);
    void write_uniform_buffer(uint32_t binding, VideoBuffer& buffer);
    void write_combined_image_sampler(uint32_t binding, Texture2D& texture);
    void write_combined_image_sampler(uint32_t binding, Cubemap& texture);
    void write_storage_image(uint32_t binding, Cubemap& texture);
    void write_storage_image(uint32_t binding, Texture2D& texture);
    void write_storage_image(uint32_t binding, VulkanImageView& image_view);
    void write_storage_buffer(uint32_t binding, VideoBuffer& buffer);
};