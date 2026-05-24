#include "material_pass_builder.h"

#include "../vulkan_shader_module.h"
#include "../vulkan_device.h"
#include "../vulkan_render_pass.h"
#include "../pipeline/vulkan_pipeline_layout.h"
#include "../pipeline/graphics_pipeline/graphics_pipeline.h"


void MaterialPassBuilder::add_vertex_binding(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate) {
    LOG_METHOD(); // Добавить проверки #TODO
    m_vertex_layout.add_binding(binding, stride, input_rate);
}

void MaterialPassBuilder::add_vertex_attribute(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset) {
    LOG_METHOD(); // Добавить проверки #TODO
    m_vertex_layout.add_attribute(location, binding, format, offset);
}

void MaterialPassBuilder::add_vertex_shader(const VulkanShaderModule& vertex_shader_module, std::string_view entry_point_name) {
    LOG_METHOD(); // Добавить проверки #TODO
    m_pipeline_builder.add_vert_shader_stage(vertex_shader_module, entry_point_name);
}

void MaterialPassBuilder::add_fragment_shader(const VulkanShaderModule& fragment_shader_module, std::string_view entry_point_name) {
    LOG_METHOD(); // Добавить проверки #TODO
    m_pipeline_builder.add_frag_shader_stage(fragment_shader_module, entry_point_name);
}

GraphicsPipeline MaterialPassBuilder::create_graphics_pipeline(
    VulkanDevice& device,
    VulkanRenderPass& render_pass,
    const VulkanPipelineLayout& pipeline_layout)
{
    LOG_METHOD(); // Добавить проверки #TODO
    m_pipeline_builder.set_graphic_objects(device, pipeline_layout, render_pass);
    m_pipeline_builder.set_vertex_layout(m_vertex_layout);
    return GraphicsPipeline(m_pipeline_builder);
}
