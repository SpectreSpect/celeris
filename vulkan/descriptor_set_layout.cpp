#include "descriptor_set_layout.h"


void DescriptorSetLayout::add_uniform_buffer(uint32_t binding, VkShaderStageFlags shader_stage_flags) {
    VkDescriptorSetLayoutBinding descriptor_set_binding{};
    descriptor_set_binding.binding = binding;
    descriptor_set_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptor_set_binding.descriptorCount = 1;
    descriptor_set_binding.stageFlags = shader_stage_flags;
    descriptor_set_binding.pImmutableSamplers = nullptr;

    descriptor_set_bindings.push_back(descriptor_set_binding);
}

void DescriptorSetLayout::add_storage_buffer(uint32_t binding, VkShaderStageFlags shader_stage_flags) {
    VkDescriptorSetLayoutBinding descriptor_set_binding{};
    descriptor_set_binding.binding = binding;
    descriptor_set_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptor_set_binding.descriptorCount = 1;
    descriptor_set_binding.stageFlags = shader_stage_flags;
    descriptor_set_binding.pImmutableSamplers = nullptr;

    descriptor_set_bindings.push_back(descriptor_set_binding);
}

void DescriptorSetLayout::add_combined_image_sampler(uint32_t binding, VkShaderStageFlags shader_stage_flags) {
    VkDescriptorSetLayoutBinding descriptor_set_binding{};
    descriptor_set_binding.binding = binding;
    descriptor_set_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_set_binding.descriptorCount = 1;
    descriptor_set_binding.stageFlags = shader_stage_flags;
    descriptor_set_binding.pImmutableSamplers = nullptr;

    descriptor_set_bindings.push_back(descriptor_set_binding);
}

void DescriptorSetLayout::add_image_storage(uint32_t binding, VkShaderStageFlags shader_stage_flags) {
    VkDescriptorSetLayoutBinding descriptor_set_binding{};
    descriptor_set_binding.binding = binding;
    descriptor_set_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptor_set_binding.descriptorCount = 1;
    descriptor_set_binding.stageFlags = shader_stage_flags;
    descriptor_set_binding.pImmutableSamplers = nullptr;

    descriptor_set_bindings.push_back(descriptor_set_binding);
}


void DescriptorSetLayout::create(VkDevice& device) {
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(descriptor_set_bindings.size());
    layoutInfo.pBindings = descriptor_set_bindings.data();

    vulkan_utils::vk_check(
        vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout),
        "vkCreateDescriptorSetLayout"
    );
}