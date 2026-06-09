#pragma once
#include <vector>
#include <vulkan/vulkan.h>
#include "vulkan_utils.h"

class VulkanEngine;

class DescriptorSetLayout {
public:
    VkDescriptorSetLayout layout;
    std::vector<VkDescriptorSetLayoutBinding> descriptor_set_bindings;

    DescriptorSetLayout() = default;

    void add_uniform_buffer(uint32_t binding, VkShaderStageFlags shader_stage_flags);
    void add_storage_buffer(uint32_t binding, VkShaderStageFlags shader_stage_flags);
    void add_combined_image_sampler(uint32_t binding, VkShaderStageFlags shader_stage_flags);
    void add_image_storage(uint32_t binding, VkShaderStageFlags shader_stage_flags);
    void create(VkDevice& device);
};