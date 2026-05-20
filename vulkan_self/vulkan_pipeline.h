#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <cstdint>

#include <glm/glm.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"
#include "vertex_layout_builder.h"

class VulkanShaderModule;
class VulkanPipelineLayout;
class VulkanRenderPass;
class VulkanDevice;
class VulkanCommandBuffer;
class DescriptorSetLayout;

struct PipelineBuliderDesc {
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

class PipelineBuilder {
private:
    static const PipelineBuliderDesc m_default_desc;
    PipelineBuliderDesc m_desc = m_default_desc;

public:
    _XCLASS_NAME(PipelineBuilder);

    PipelineBuilder() = default;

    PipelineBuilder& set_device(const VulkanDevice& device) noexcept;
    PipelineBuilder& set_layout(const VulkanPipelineLayout& pipeline_layout) noexcept;
    PipelineBuilder& set_render_pass(const VulkanRenderPass& render_pass) noexcept;
    PipelineBuilder& set_graphic_objects(
        const VulkanDevice& device,
        const VulkanPipelineLayout& pipeline_layout,
        const VulkanRenderPass& render_pass
    ) noexcept;

    PipelineBuilder& add_vert_shader_stage(
        const VulkanShaderModule& vertex_shader_module,
        std::string_view entry_point_name = m_default_desc.vertex_entry_point_name
    );

    PipelineBuilder& add_frag_shader_stage(
        const VulkanShaderModule& fragment_shader_module,
        std::string_view entry_point_name = m_default_desc.fragment_entry_point_name
    );

    

    PipelineBuilder& set_vertex_layout(const VertexLayoutBuilder& layout) noexcept;

    PipelineBuilder& set_input_assembly(
        VkPrimitiveTopology topology = m_default_desc.topology,
        bool primitive_restart_enable = m_default_desc.primitive_restart_enable
    ) noexcept;

    PipelineBuilder& set_rasterizer(
        bool depth_clamp_enable = m_default_desc.depth_clamp_enable,
        bool rasterizer_discard_enable = m_default_desc.rasterizer_discard_enable,
        VkPolygonMode polygon_mode = m_default_desc.polygon_mode,
        float line_width = m_default_desc.line_width,
        VkCullModeFlags cull_mode = m_default_desc.cull_mode, // Потом не забыть поменять!!! #TODO
        VkFrontFace front_face = m_default_desc.front_face,
        VkBool32 depth_bias_enable = m_default_desc.depth_bias_enable
    ) noexcept;

    PipelineBuilder& set_multisampling(
        bool sample_shading_enable = m_default_desc.sample_shading_enable,
        VkSampleCountFlagBits rasterization_samples = m_default_desc.rasterization_samples // Предполагаю должно быть больше для нормальной работы? #TODO
    ) noexcept;

    // Опять же, пока что всё по простому, чтобы не тратить просто так время. Кода пригодится - сделаю полноценно.
    PipelineBuilder& set_color_blending(
        VkColorComponentFlags color_write_mask = m_default_desc.color_write_mask,
        bool blend_enable = m_default_desc.blend_enable
    ) noexcept;

    const PipelineBuliderDesc& desc() const noexcept;
};

class VulkanPipeline {
public:
    _XCLASS_NAME(VulkanPipeline);

    explicit VulkanPipeline(const PipelineBuilder& builder);
    ~VulkanPipeline() noexcept;
    void destroy() noexcept;

    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    VulkanPipeline(VulkanPipeline&& other) noexcept;
    VulkanPipeline& operator=(VulkanPipeline&& other) noexcept;

    VkPipeline handle() const noexcept;

    void bind(
        VulkanCommandBuffer& command_buffer, 
        VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS
    ) const;

    static PipelineBuilder create_builder() noexcept; // Микро функция для удобства

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

private:
    
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
};
