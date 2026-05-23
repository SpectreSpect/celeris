#include "material_pass_builder.h"

void MaterialPassBuilder::add_uniform_buffer(uint32_t binding, VkShaderStageFlags shader_stage_flags) {
    m_material_dsl_builder.add_uniform_buffer(binding, shader_stage_flags);
}

void MaterialPassBuilder::add_combined_image_sampler(uint32_t binding, VkShaderStageFlags shader_stage_flags) {
    m_material_dsl_builder.add_combined_image_sampler(binding, shader_stage_flags);
}

void MaterialPassBuilder::add_push_constants(uint32_t size_bytes, uint32_t offset, VkShaderStageFlags stage_flags) {
    m_pipeline_layout_builder.add_push_constants(size_bytes, offset, stage_flags);
}

void MaterialPassBuilder::add_descriptor_set_layout(const DescriptorSetLayout& layout) {
    m_pipeline_layout_builder.add_descriptor_set_layout(layout);
}

void MaterialPassBuilder::add_vertex_binding(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate) {
    m_vertex_layout.add_binding(binding, stride, input_rate);
}

void MaterialPassBuilder::add_vertex_attribute(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset) {
    m_vertex_layout.add_attribute(location, binding, format, offset);
}

void MaterialPassBuilder::add_vertex_shader(const VulkanShaderModule& vertex_shader_module, std::string_view entry_point_name) {
    m_pipeline_builder.add_vert_shader_stage(vertex_shader_module, entry_point_name);
}

void MaterialPassBuilder::add_fragment_shader(const VulkanShaderModule& fragment_shader_module, std::string_view entry_point_name) {
    m_pipeline_builder.add_frag_shader_stage(fragment_shader_module, entry_point_name);
}

DescriptorSetLayout MaterialPassBuilder::create_descriptor_set_layout(VulkanDevice& device) {
    return DescriptorSetLayout(device, m_material_dsl_builder);
}

VulkanPipelineLayout MaterialPassBuilder::create_pipeline_layout(VulkanDevice& device, const DescriptorSetLayout& material_ds_layout) {
    m_pipeline_layout_builder.set_device(device);
    m_pipeline_layout_builder.prepend_descriptor_set_layout(material_ds_layout);
    return VulkanPipelineLayout(m_pipeline_layout_builder);
}

GraphicsPipeline MaterialPassBuilder::create_graphics_pipeline(VulkanDevice& device, VulkanRenderPass& render_pass, VulkanPipelineLayout& pipeline_layout) {
    m_pipeline_builder.set_graphic_objects(device, pipeline_layout, render_pass);
    m_pipeline_builder.set_vertex_layout(m_vertex_layout);
    return GraphicsPipeline(m_pipeline_builder);
}

DescriptorSetLayoutBuilder MaterialPassBuilder::material_dsl_builder() const {
    return m_material_dsl_builder;
}