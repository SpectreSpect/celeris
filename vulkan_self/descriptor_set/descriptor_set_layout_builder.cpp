#include "descriptor_set_layout_builder.h"

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::set_flags(VkDescriptorSetLayoutCreateFlags flags) noexcept {
    m_flags = flags;

    return *this;
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::add(uint32_t binding, VkDescriptorType type, 
                                                            VkShaderStageFlags shader_stage_flags, uint32_t descriptor_count) {
    LOG_METHOD();

    logger.check(descriptor_count > 0, "Descriptor set layout binding must contain at least one descriptor");
    
    VkDescriptorSetLayoutBinding desc{};
    desc.binding = binding;
    desc.descriptorType = type;
    desc.descriptorCount = descriptor_count;
    desc.stageFlags = shader_stage_flags;
    desc.pImmutableSamplers = nullptr;

    m_bindings.push_back(desc);

    return *this;
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::add_uniform_buffer(uint32_t binding, VkShaderStageFlags shader_stage_flags) {
    LOG_METHOD();
    add(binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shader_stage_flags);

    return *this;
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::add_storage_buffer(uint32_t binding, VkShaderStageFlags shader_stage_flags) {
    LOG_METHOD();
    add(binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, shader_stage_flags);

    return *this;
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::add_combined_image_sampler(uint32_t binding, VkShaderStageFlags shader_stage_flags) {
    LOG_METHOD();
    add(binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, shader_stage_flags);

    return *this;
}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::add_storage_image(uint32_t binding, VkShaderStageFlags shader_stage_flags) {
    LOG_METHOD();
    add(binding, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, shader_stage_flags);

    return *this;
}

std::span<const VkDescriptorSetLayoutBinding> DescriptorSetLayoutBuilder::get_bindings() const noexcept{
    return m_bindings;
}

VkDescriptorSetLayoutCreateFlags DescriptorSetLayoutBuilder::flags() const noexcept {
    return m_flags;
}