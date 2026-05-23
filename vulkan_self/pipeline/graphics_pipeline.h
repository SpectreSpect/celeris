#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <cstdint>

#include <glm/glm.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../logger/logger_header.h"
#include "../vertex_layout_builder.h"
#include "pipeline.h"

class VulkanShaderModule;
class VulkanPipelineLayout;
class VulkanRenderPass;
class VulkanDevice;
class VulkanCommandBuffer;
class DescriptorSetLayout;
class VulkanEngine;

struct GraphicsPipelineBuliderDesc {
    VkDevice device;
    VkPipelineLayout pipeline_layout;
    VkRenderPass render_pass;

    VkShaderModule vertex_shader_module;
    std::string vertex_entry_point_name;

    VkShaderModule fragment_shader_module;
    std::string fragment_entry_point_name;

    std::vector<VkVertexInputBindingDescription> vertex_bindings;
    std::vector<VkVertexInputAttributeDescription> vertex_attributes;

    VkPrimitiveTopology topology;
    bool primitive_restart_enable;

    bool depth_clamp_enable;
    bool rasterizer_discard_enable;
    VkPolygonMode polygon_mode;
    float line_width;
    VkCullModeFlags cull_mode;
    VkFrontFace front_face;
    VkBool32 depth_bias_enable;

    bool sample_shading_enable;
    VkSampleCountFlagBits rasterization_samples;

    VkColorComponentFlags color_write_mask;
    bool blend_enable;
};

class GraphicsPipelineBuilder {
private:
    GraphicsPipelineBuliderDesc m_desc = default_desc;

public:
    _XCLASS_NAME(GraphicsPipelineBuilder);

    static const GraphicsPipelineBuliderDesc default_desc;

    GraphicsPipelineBuilder() = default;

    GraphicsPipelineBuilder& set_device(const VulkanDevice& device) noexcept;
    GraphicsPipelineBuilder& set_layout(const VulkanPipelineLayout& pipeline_layout) noexcept;
    GraphicsPipelineBuilder& set_render_pass(const VulkanRenderPass& render_pass) noexcept;
    GraphicsPipelineBuilder& set_graphic_objects(
        const VulkanDevice& device,
        const VulkanPipelineLayout& pipeline_layout,
        const VulkanRenderPass& render_pass
    ) noexcept;

    GraphicsPipelineBuilder& add_vert_shader_stage(
        const VulkanShaderModule& vertex_shader_module,
        std::string_view entry_point_name = default_desc.vertex_entry_point_name
    );

    GraphicsPipelineBuilder& add_frag_shader_stage(
        const VulkanShaderModule& fragment_shader_module,
        std::string_view entry_point_name = default_desc.fragment_entry_point_name
    );

    GraphicsPipelineBuilder& set_vertex_layout(const VertexLayoutBuilder& layout) noexcept;

    GraphicsPipelineBuilder& set_input_assembly(
        VkPrimitiveTopology topology = default_desc.topology,
        bool primitive_restart_enable = default_desc.primitive_restart_enable
    ) noexcept;

    GraphicsPipelineBuilder& set_rasterizer(
        bool depth_clamp_enable = default_desc.depth_clamp_enable,
        bool rasterizer_discard_enable = default_desc.rasterizer_discard_enable,
        VkPolygonMode polygon_mode = default_desc.polygon_mode,
        float line_width = default_desc.line_width,
        VkCullModeFlags cull_mode = default_desc.cull_mode, // Потом не забыть поменять!!! #TODO
        VkFrontFace front_face = default_desc.front_face,
        VkBool32 depth_bias_enable = default_desc.depth_bias_enable
    ) noexcept;

    GraphicsPipelineBuilder& set_multisampling(
        bool sample_shading_enable = default_desc.sample_shading_enable,
        VkSampleCountFlagBits rasterization_samples = default_desc.rasterization_samples // Предполагаю должно быть больше для нормальной работы? #TODO
    ) noexcept;

    // Опять же, пока что всё по простому, чтобы не тратить просто так время. Кода пригодится - сделаю полноценно.
    GraphicsPipelineBuilder& set_color_blending(
        VkColorComponentFlags color_write_mask = default_desc.color_write_mask,
        bool blend_enable = default_desc.blend_enable
    ) noexcept;

    const GraphicsPipelineBuliderDesc& desc() const noexcept;
};

class GraphicsPipeline : public Pipeline {
public:
    _XCHILD_NAME(GraphicsPipeline);

    explicit GraphicsPipeline(const GraphicsPipelineBuilder& builder);

    VkPipelineBindPoint get_bind_point() const noexcept override;

    static GraphicsPipelineBuilder create_builder() noexcept; // Микро функция для удобства

    static void set_y_down_viewport(
        VulkanCommandBuffer& command_buffer, 
        glm::vec2 size,
        glm::vec2 origin = glm::vec2{0.0f, 0.0f},
        float min_depth = 0.0f,
        float max_depth = 1.0f
    );

    static void set_y_down_viewport(
        VulkanCommandBuffer& command_buffer, 
        VkExtent2D size,
        VkOffset2D origin = VkOffset2D{0, 0},
        float min_depth = 0.0f,
        float max_depth = 1.0f
    );

    static void set_y_up_viewport(
        VulkanCommandBuffer& command_buffer, 
        glm::vec2 size,
        glm::vec2 origin = glm::vec2{0.0f, 0.0f},
        float min_depth = 0.0f,
        float max_depth = 1.0f
    );

    static void set_y_up_viewport(
        VulkanCommandBuffer& command_buffer, 
        VkExtent2D size,
        VkOffset2D origin = VkOffset2D{0, 0},
        float min_depth = 0.0f,
        float max_depth = 1.0f
    );

    static void set_scissor(
        VulkanCommandBuffer& command_buffer,
        VkExtent2D extent,
        VkOffset2D offset = VkOffset2D{0, 0}
    );

    static void set_y_down_viewport(
        VulkanCommandBuffer& command_buffer,
        VulkanEngine& engine,
        VkOffset2D origin = VkOffset2D{0, 0},
        float min_depth = 0.0f,
        float max_depth = 1.0f
    );

    static void set_y_up_viewport(
        VulkanCommandBuffer& command_buffer,
        VulkanEngine& engine,
        VkOffset2D origin = VkOffset2D{0, 0},
        float min_depth = 0.0f,
        float max_depth = 1.0f
    );

    static void set_scissor(
        VulkanCommandBuffer& command_buffer,
        VulkanEngine& engine,
        VkOffset2D offset = VkOffset2D{0, 0}
    );

};
