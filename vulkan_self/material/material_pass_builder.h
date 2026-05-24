#pragma once
#include "../descriptor_set/descriptor_set_layout_builder.h"
#include "../pipeline/vulkan_pipeline_layout.h"
#include "../vertex_layout_builder.h"
#include "../pipeline/graphics_pipeline.h"
#include "../descriptor_set/descriptor_set_layout.h"

class MaterialPassBuilder {
public:
    MaterialPassBuilder() = default;

    void add_uniform_buffer(uint32_t binding, VkShaderStageFlags shader_stage_flags);
    void add_storage_buffer(uint32_t binding, VkShaderStageFlags shader_stage_flags);
    void add_combined_image_sampler(uint32_t binding, VkShaderStageFlags shader_stage_flags);
    void add_push_constants(uint32_t size_bytes, uint32_t offset = 0, VkShaderStageFlags stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    void add_descriptor_set_layout(const DescriptorSetLayout& layout);
    // add_descriptor_set_layout();
    void add_vertex_binding(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate = VK_VERTEX_INPUT_RATE_VERTEX);
    void add_vertex_attribute(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset);
    void add_vertex_shader(const VulkanShaderModule& vertex_shader_module, 
                           std::string_view entry_point_name = GraphicsPipelineBuilder::default_desc.vertex_entry_point_name);
    void add_fragment_shader(const VulkanShaderModule& fragment_shader_module, 
                             std::string_view entry_point_name = GraphicsPipelineBuilder::default_desc.fragment_entry_point_name);

    DescriptorSetLayout create_descriptor_set_layout(VulkanDevice& device);
    VulkanPipelineLayout create_pipeline_layout(VulkanDevice& device, const DescriptorSetLayout& material_ds_layout);
    GraphicsPipeline create_graphics_pipeline(VulkanDevice& device, VulkanRenderPass& render_pass, VulkanPipelineLayout& pipeline_layout);

    DescriptorSetLayoutBuilder material_dsl_builder() const;
private:
    DescriptorSetLayoutBuilder m_material_dsl_builder;
    PipelineLayoutBuilder m_pipeline_layout_builder;
    VertexLayoutBuilder m_vertex_layout;
    GraphicsPipelineBuilder m_pipeline_builder;
};