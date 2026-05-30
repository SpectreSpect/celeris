#include "pipeline_pass_builder.h"

#include "../descriptor_set/descriptor_set_layout.h"
#include "vulkan_pipeline_layout.h"
#include "../vulkan_device.h"

void PipelinePassBuilder::add_uniform_buffer(uint32_t binding, VkShaderStageFlags shader_stage_flags) {
    m_material_dsl_builder.add_uniform_buffer(binding, shader_stage_flags);
}

void PipelinePassBuilder::add_storage_buffer(uint32_t binding, VkShaderStageFlags shader_stage_flags) {
    m_material_dsl_builder.add_storage_buffer(binding, shader_stage_flags);
}

void PipelinePassBuilder::add_combined_image_sampler(uint32_t binding, VkShaderStageFlags shader_stage_flags) {
    m_material_dsl_builder.add_combined_image_sampler(binding, shader_stage_flags);
}

void PipelinePassBuilder::add_storage_image(uint32_t binding, VkShaderStageFlags shader_stage_flags) {
    m_material_dsl_builder.add_storage_image(binding, shader_stage_flags);
}

void PipelinePassBuilder::add_push_constants(uint32_t size_bytes, uint32_t offset, VkShaderStageFlags stage_flags) {
    m_pipeline_layout_builder.add_push_constants(size_bytes, offset, stage_flags);
}

void PipelinePassBuilder::add_descriptor_set_layout(const DescriptorSetLayout& layout) {
    m_pipeline_layout_builder.add_descriptor_set_layout(layout);
}

const DescriptorSetLayoutBuilder& PipelinePassBuilder::material_dsl_builder() const {
    return m_material_dsl_builder;
}

const PipelineLayoutBuilder& PipelinePassBuilder::pipeline_layout_builder() const {
    return m_pipeline_layout_builder;
}

DescriptorSetLayout PipelinePassBuilder::create_descriptor_set_layout(VulkanDevice& device) {
    return DescriptorSetLayout(device, m_material_dsl_builder);
}

VulkanPipelineLayout PipelinePassBuilder::create_pipeline_layout(VulkanDevice& device, const DescriptorSetLayout& material_ds_layout) {
    m_pipeline_layout_builder.set_device(device);
    m_pipeline_layout_builder.prepend_descriptor_set_layout(material_ds_layout);
    return VulkanPipelineLayout(m_pipeline_layout_builder);
}
