#pragma once

#include <vector>

#include <vulkan/vulkan.h>
#include "../logger/logger_header.h"
#include "../shader_stages.h"

class DescriptorSetLayoutBuilder {
public:
    _XCLASS_NAME(DescriptorSetLayout);

    DescriptorSetLayoutBuilder() = default;

    DescriptorSetLayoutBuilder(const DescriptorSetLayoutBuilder&) = delete;
    DescriptorSetLayoutBuilder& operator=(const DescriptorSetLayoutBuilder&) = delete;

    DescriptorSetLayoutBuilder& add(uint32_t binding, VkDescriptorType type, VkShaderStageFlags shader_stage_flags, uint32_t descriptor_count = 1);
    DescriptorSetLayoutBuilder& add_uniform_buffer(uint32_t binding, VkShaderStageFlags shader_stage_flags);
    DescriptorSetLayoutBuilder& add_storage_buffer(uint32_t binding, VkShaderStageFlags shader_stage_flags);
    DescriptorSetLayoutBuilder& add_combined_image_sampler(uint32_t binding, VkShaderStageFlags shader_stage_flags);
    DescriptorSetLayoutBuilder& add_storage_image(uint32_t binding, VkShaderStageFlags shader_stage_flags);
private:
    std::vector<VkDescriptorSetLayoutBinding> bindings;
};
