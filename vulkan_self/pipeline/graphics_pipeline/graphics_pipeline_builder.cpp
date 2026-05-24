#include "graphics_pipeline_builder.h"

#include "../../vulkan_device.h"
#include "../vulkan_pipeline_layout.h"
#include "../../vulkan_render_pass.h"
#include "../../vulkan_shader_module.h"
#include "../../vertex_layout_builder.h"

const GraphicsPipelineBuliderDesc GraphicsPipelineBuilder::default_desc = {
    .device = VK_NULL_HANDLE,
    .pipeline_layout = VK_NULL_HANDLE,
    .render_pass = VK_NULL_HANDLE,

    .vertex_shader_module = VK_NULL_HANDLE,
    .vertex_entry_point_name = "main",

    .fragment_shader_module = VK_NULL_HANDLE,
    .fragment_entry_point_name = "main",

    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitive_restart_enable = false,

    .depth_clamp_enable = false,
    .rasterizer_discard_enable = false,
    .polygon_mode = VK_POLYGON_MODE_FILL,
    .line_width = 1.0f,
    .cull_mode = VK_CULL_MODE_NONE, // Потом не забыть поменять!!! #TODO
    .front_face = VK_FRONT_FACE_CLOCKWISE,
    .depth_bias_enable = VK_FALSE,

    .sample_shading_enable = false,
    .rasterization_samples = VK_SAMPLE_COUNT_1_BIT,

    .color_write_mask = 
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT,
    .blend_enable = false
};

GraphicsPipelineBuilder& GraphicsPipelineBuilder::set_device(const VulkanDevice& device) noexcept {
    m_desc.device = device.handle();

    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::set_layout(const VulkanPipelineLayout& pipeline_layout) noexcept {
    m_desc.pipeline_layout = pipeline_layout.handle();

    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::set_render_pass(const VulkanRenderPass& render_pass) noexcept {
    m_desc.render_pass = render_pass.handle();

    return *this;
}
GraphicsPipelineBuilder& GraphicsPipelineBuilder::set_graphic_objects(
    const VulkanDevice& device,
    const VulkanPipelineLayout& pipeline_layout,
    const VulkanRenderPass& render_pass) noexcept
{
    set_device(device);
    set_layout(pipeline_layout);
    set_render_pass(render_pass);

    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::add_vert_shader_stage(
    const VulkanShaderModule& vertex_shader_module,
    std::string_view entry_point_name)
{
    m_desc.vertex_shader_module = vertex_shader_module.handle();
    m_desc.vertex_entry_point_name = entry_point_name; // копирование

    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::add_frag_shader_stage(
    const VulkanShaderModule& fragment_shader_module,
    std::string_view entry_point_name)
{
    m_desc.fragment_shader_module = fragment_shader_module.handle();
    m_desc.fragment_entry_point_name = entry_point_name; // копирование

    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::set_vertex_layout(const VertexLayoutBuilder& layout) noexcept
{
    m_desc.vertex_bindings = layout.bindings();
    m_desc.vertex_attributes = layout.attributes();
    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::set_input_assembly(
    VkPrimitiveTopology topology,
    bool primitive_restart_enable) noexcept
{
    m_desc.topology = topology;
    m_desc.primitive_restart_enable = primitive_restart_enable;

    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::set_rasterizer(
    bool depth_clamp_enable,
    bool rasterizer_discard_enable,
    VkPolygonMode polygon_mode,
    float line_width,
    VkCullModeFlags cull_mode,
    VkFrontFace front_face,
    VkBool32 depth_bias_enable) noexcept
{
    m_desc.depth_clamp_enable = depth_clamp_enable;
    m_desc.rasterizer_discard_enable = rasterizer_discard_enable;
    m_desc.polygon_mode = polygon_mode;
    m_desc.line_width = line_width;
    m_desc.cull_mode = cull_mode;
    m_desc.front_face = front_face;
    m_desc.depth_bias_enable = depth_bias_enable;

    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::set_multisampling(
    bool sample_shading_enable,
    VkSampleCountFlagBits rasterization_samples) noexcept
{
    m_desc.sample_shading_enable = sample_shading_enable;
    m_desc.rasterization_samples = rasterization_samples;

    return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::set_color_blending(
    VkColorComponentFlags color_write_mask,
    bool blend_enable) noexcept
{
    m_desc.color_write_mask = color_write_mask;
    m_desc.blend_enable = blend_enable;

    return *this;
}

const GraphicsPipelineBuliderDesc& GraphicsPipelineBuilder::desc() const noexcept {
    return m_desc;
}
