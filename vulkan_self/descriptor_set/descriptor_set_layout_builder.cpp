#include "descriptor_set_layout_builder.h"

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::add(uint32_t binding, VkDescriptorType type, 
                                                            VkShaderStageFlags shader_stage_flags, uint32_t descriptor_count) {
    LOG_METHOD();
    
    VkDescriptorSetLayoutBinding desc{};
    desc.binding = binding;
    desc.descriptorType = type;
    desc.descriptorCount = descriptor_count;
    desc.stageFlags = shader_stage_flags;
    desc.pImmutableSamplers = nullptr;

    bindings.push_back(desc);
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::add_uniform_buffer(uint32_t binding, VkShaderStageFlags shader_stage_flags) {
    LOG_METHOD();
    add(binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shader_stage_flags);
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::add_storage_buffer(uint32_t binding, VkShaderStageFlags shader_stage_flags) {
    LOG_METHOD();
    add(binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shader_stage_flags);
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::add_combined_image_sampler(uint32_t binding, VkShaderStageFlags shader_stage_flags) {
    LOG_METHOD();
    add(binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, shader_stage_flags);
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::add_storage_image(uint32_t binding, VkShaderStageFlags shader_stage_flags) {
    LOG_METHOD();
    add(binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, shader_stage_flags);
}