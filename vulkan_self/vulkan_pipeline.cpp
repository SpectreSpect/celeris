#include "vulkan_pipeline.h"

#include <utility>

#include "vulkan_shader_module.h"
#include "vulkan_pipeline_layout.h"
#include "vulkan_render_pass.h"
#include "vulkan_device.h"
#include "vulkan_command_buffer.h"

const PipelineBuliderDesc PipelineBuilder::m_default_desc = {
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

PipelineBuilder& PipelineBuilder::set_device(const VulkanDevice& device) noexcept {
    m_desc.device = device.handle();

    return *this;
}

PipelineBuilder& PipelineBuilder::set_layout(const VulkanPipelineLayout& pipeline_layout) noexcept {
    m_desc.pipeline_layout = pipeline_layout.handle();

    return *this;
}

PipelineBuilder& PipelineBuilder::set_render_pass(const VulkanRenderPass& render_pass) noexcept {
    m_desc.render_pass = render_pass.handle();

    return *this;
}
PipelineBuilder& PipelineBuilder::set_graphic_objects(
    const VulkanDevice& device,
    const VulkanPipelineLayout& pipeline_layout,
    const VulkanRenderPass& render_pass) noexcept
{
    set_device(device);
    set_layout(pipeline_layout);
    set_render_pass(render_pass);

    return *this;
}

PipelineBuilder& PipelineBuilder::add_vert_shader_stage(
    const VulkanShaderModule& vertex_shader_module,
    std::string_view entry_point_name)
{
    m_desc.vertex_shader_module = vertex_shader_module.handle();
    m_desc.vertex_entry_point_name = entry_point_name; // копирование

    return *this;
}

PipelineBuilder& PipelineBuilder::add_frag_shader_stage(
    const VulkanShaderModule& fragment_shader_module,
    std::string_view entry_point_name)
{
    m_desc.fragment_shader_module = fragment_shader_module.handle();
    m_desc.fragment_entry_point_name = entry_point_name; // копирование

    return *this;
}

PipelineBuilder& PipelineBuilder::set_vertex_layout() noexcept
{
    // #TODO

    return *this;
}

PipelineBuilder& PipelineBuilder::set_input_assembly(
    VkPrimitiveTopology topology,
    bool primitive_restart_enable) noexcept
{
    m_desc.topology = topology;
    m_desc.primitive_restart_enable = primitive_restart_enable;

    return *this;
}

PipelineBuilder& PipelineBuilder::set_rasterizer(
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

PipelineBuilder& PipelineBuilder::set_multisampling(
    bool sample_shading_enable,
    VkSampleCountFlagBits rasterization_samples) noexcept
{
    m_desc.sample_shading_enable = sample_shading_enable;
    m_desc.rasterization_samples = rasterization_samples;

    return *this;
}

PipelineBuilder& PipelineBuilder::set_color_blending(
    VkColorComponentFlags color_write_mask,
    bool blend_enable) noexcept
{
    m_desc.color_write_mask = color_write_mask;
    m_desc.blend_enable = blend_enable;

    return *this;
}

const PipelineBuliderDesc& PipelineBuilder::desc() const noexcept {
    return m_desc;
}

VulkanPipeline::VulkanPipeline(const PipelineBuilder& builder)
    : m_device(builder.desc().device)
{
    LOG_METHOD();

    logger.check(m_device != VK_NULL_HANDLE)
        << "Device is not initialized. Init it or specify in methods "
        << VSCODE_CLR_STREAM("PipelineBulider", "set_device") << " or "
        << VSCODE_CLR_STREAM("PipelineBulider", "set_graphic_objects") << "\n";
    
    logger.check(builder.desc().pipeline_layout)
        << "Layout is not initialized. Init it or specify in methods "
        << VSCODE_CLR_STREAM("PipelineBulider", "set_layout") << " or "
        << VSCODE_CLR_STREAM("PipelineBulider", "set_graphic_objects") << "\n";
    
    logger.check(builder.desc().render_pass)
        << "RenderPass is not initialized. Init it or specify in methods "
        << VSCODE_CLR_STREAM("PipelineBulider", "set_render_pass") << " or "
        << VSCODE_CLR_STREAM("PipelineBulider", "set_graphic_objects") << "\n";

    logger.check(builder.desc().vertex_shader_module != VK_NULL_HANDLE) 
        << "Vertex shader module is not initialized. Use "
        << VSCODE_CLR_STREAM("PipelineBulider", "add_vert_shader_stage") << ".\n";

    logger.check(builder.desc().fragment_shader_module != VK_NULL_HANDLE)
        << "Fragment shader module is not initialized. Use "
        << VSCODE_CLR_STREAM("PipelineBulider", "add_frag_shader_stage") << ".\n";
    
    VkPipelineShaderStageCreateInfo vert_shader_stage_info{};
    vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = builder.desc().vertex_shader_module;
    vert_shader_stage_info.pName = builder.desc().vertex_entry_point_name.c_str();

    VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
    frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = builder.desc().fragment_shader_module;
    frag_shader_stage_info.pName = builder.desc().fragment_entry_point_name.c_str();

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        vert_shader_stage_info,
        frag_shader_stage_info
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.pVertexBindingDescriptions = nullptr;
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    vertex_input_info.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = builder.desc().topology;
    input_assembly.primitiveRestartEnable = builder.desc().primitive_restart_enable;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = builder.desc().depth_clamp_enable;
    rasterizer.rasterizerDiscardEnable = builder.desc().rasterizer_discard_enable;
    rasterizer.polygonMode = builder.desc().polygon_mode;
    rasterizer.lineWidth = builder.desc().line_width;
    rasterizer.cullMode = builder.desc().cull_mode;
    rasterizer.frontFace = builder.desc().front_face;
    rasterizer.depthBiasEnable = builder.desc().depth_bias_enable;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = builder.desc().sample_shading_enable;
    multisampling.rasterizationSamples = builder.desc().rasterization_samples;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = builder.desc().color_write_mask;
    color_blend_attachment.blendEnable = builder.desc().blend_enable;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    color_blending.blendConstants[0] = 0.0f;
    color_blending.blendConstants[1] = 0.0f;
    color_blending.blendConstants[2] = 0.0f;
    color_blending.blendConstants[3] = 0.0f;

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = nullptr;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.layout = builder.desc().pipeline_layout;
    pipeline_info.renderPass = builder.desc().render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    VkResult result = vkCreateGraphicsPipelines(
        m_device,
        VK_NULL_HANDLE,
        1,
        &pipeline_info,
        nullptr,
        &m_pipeline
    );

    logger.check(result == VK_SUCCESS, "Failed to create graphics pipeline");
}

VulkanPipeline::~VulkanPipeline() noexcept {
    destroy();
}

void VulkanPipeline::destroy() noexcept {
    if (m_device != VK_NULL_HANDLE && m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
    }

    m_device = VK_NULL_HANDLE;
    m_pipeline = VK_NULL_HANDLE;
}

VulkanPipeline::VulkanPipeline(VulkanPipeline&& other) noexcept
    :   m_pipeline(std::exchange(other.m_pipeline, VK_NULL_HANDLE)),
        m_device(std::exchange(other.m_device, VK_NULL_HANDLE)) {}

VulkanPipeline& VulkanPipeline::operator=(VulkanPipeline&& other) noexcept {
    if (this != &other) {
        destroy();

        m_pipeline = std::exchange(other.m_pipeline, VK_NULL_HANDLE);
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    }

    return *this;
}

VkPipeline VulkanPipeline::handle() const noexcept {
    return m_pipeline;
}

void VulkanPipeline::bind(VulkanCommandBuffer& command_buffer, VkPipelineBindPoint bind_point) const {
    LOG_METHOD();

    logger.check(m_pipeline != VK_NULL_HANDLE, "Pipeline is not initialized");
    logger.check(command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initialized");

    vkCmdBindPipeline(
        command_buffer.handle(),
        bind_point,
        m_pipeline
    );
}

PipelineBuilder VulkanPipeline::create_builder() noexcept {
    return PipelineBuilder();
}

void VulkanPipeline::set_viewport(
    VulkanCommandBuffer& command_buffer, 
    glm::vec2 size,
    glm::vec2 origin,
    float min_depth,
    float max_depth) 
{
    LOG_NAMED("VulkanPipeline");

    logger.check(command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initialized");

    VkViewport viewport{};
    viewport.x = origin.x;
    viewport.y = origin.y;
    viewport.width = size.x;
    viewport.height = size.y;
    viewport.minDepth = min_depth;
    viewport.maxDepth = max_depth;

    vkCmdSetViewport(command_buffer.handle(), 0, 1, &viewport);
}

void VulkanPipeline::set_viewport(
    VulkanCommandBuffer& command_buffer, 
    VkExtent2D size,
    VkOffset2D origin,
    float min_depth,
    float max_depth)
{
    set_viewport(
        command_buffer,
        Utils::to_vec2(size),
        Utils::to_vec2(origin),
        min_depth,
        max_depth
    );
}

void VulkanPipeline::set_scissor(
    VulkanCommandBuffer& command_buffer,
    VkExtent2D extent,
    VkOffset2D offset)
{
    LOG_NAMED("VulkanPipeline");

    logger.check(command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initialized");

    VkRect2D scissor{};
    scissor.offset = offset;
    scissor.extent = extent;

    vkCmdSetScissor(command_buffer.handle(), 0, 1, &scissor);
}
