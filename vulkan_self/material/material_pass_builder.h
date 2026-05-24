#pragma once

#include <cstdint>
#include <string_view>
#include <vulkan/vulkan.h>
#include "../logger/logger_header.h"

#include "../pipeline/pipeline_pass_builder.h"
#include "../pipeline/graphics_pipeline/graphics_pipeline_builder.h"
#include "../vertex_layout_builder.h"

class VulkanShaderModule;
class VulkanDevice;
class VulkanRenderPass;
class VulkanPipelineLayout;
class GraphicsPipeline; 

class MaterialPassBuilder : public PipelinePassBuilder {
public:
    _XCHILD_NAME(MaterialPassBuilder)

    MaterialPassBuilder() = default;

    void add_vertex_binding(uint32_t binding, uint32_t stride, VkVertexInputRate input_rate = VK_VERTEX_INPUT_RATE_VERTEX);
    void add_vertex_attribute(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset);
    void add_vertex_shader(const VulkanShaderModule& vertex_shader_module, 
                           std::string_view entry_point_name = GraphicsPipelineBuilder::default_desc.vertex_entry_point_name);
    void add_fragment_shader(const VulkanShaderModule& fragment_shader_module, 
                             std::string_view entry_point_name = GraphicsPipelineBuilder::default_desc.fragment_entry_point_name);

    GraphicsPipeline create_graphics_pipeline(VulkanDevice& device, VulkanRenderPass& render_pass, const VulkanPipelineLayout& pipeline_layout);

private:
    VertexLayoutBuilder m_vertex_layout;
    GraphicsPipelineBuilder m_pipeline_builder;
};
