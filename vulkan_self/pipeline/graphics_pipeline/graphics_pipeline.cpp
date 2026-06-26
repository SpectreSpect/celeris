#include "graphics_pipeline.h"

#include <utility>

#include "graphics_pipeline_builder.h"
#include "../../vulkan_shader_module.h"
#include "../vulkan_pipeline_layout.h"
#include "../../vulkan_render_pass.h"
#include "../../vulkan_device.h"
#include "../../vulkan_command_buffer.h"
#include "../../descriptor_set/descriptor_set_layout.h"
#include "../../vulkan_engine.h"

GraphicsPipeline::GraphicsPipeline(const GraphicsPipelineBuilder& builder)
{
    VkDevice device = builder.desc().device;
    VkPipeline pipeline = VK_NULL_HANDLE;

    LOG_METHOD();

    logger.check(device != VK_NULL_HANDLE)
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
    vertex_input_info.vertexBindingDescriptionCount = 
        static_cast<uint32_t>(builder.desc().vertex_bindings.size());

    vertex_input_info.pVertexBindingDescriptions = builder.desc().vertex_bindings.data();

    vertex_input_info.vertexAttributeDescriptionCount = 
        static_cast<uint32_t>(builder.desc().vertex_attributes.size());

    vertex_input_info.pVertexAttributeDescriptions = builder.desc().vertex_attributes.data();

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

    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.minDepthBounds = 0.0f;
    depth_stencil.maxDepthBounds = 1.0f;
    depth_stencil.stencilTestEnable = VK_FALSE;

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
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.layout = builder.desc().pipeline_layout;
    pipeline_info.renderPass = builder.desc().render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    VkResult result = vkCreateGraphicsPipelines(
        device,
        VK_NULL_HANDLE,
        1,
        &pipeline_info,
        nullptr,
        &pipeline
    );

    logger.check(result == VK_SUCCESS, "Failed to create graphics pipeline");

    set_pipeline(device, pipeline, builder.desc().pipeline_layout);
}

VkPipelineBindPoint GraphicsPipeline::get_bind_point() const noexcept {
    return VK_PIPELINE_BIND_POINT_GRAPHICS;
}

GraphicsPipelineBuilder GraphicsPipeline::create_builder() noexcept {
    return GraphicsPipelineBuilder();
}

void GraphicsPipeline::set_y_down_viewport(
    VulkanCommandBuffer& command_buffer, 
    glm::vec2 size,
    glm::vec2 origin,
    float min_depth,
    float max_depth) 
{
    LOG_NAMED("GraphicsPipeline");

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

void GraphicsPipeline::set_y_down_viewport(
    VulkanCommandBuffer& command_buffer, 
    VkExtent2D size,
    VkOffset2D origin,
    float min_depth,
    float max_depth)
{
    set_y_down_viewport(
        command_buffer,
        Utils::to_vec2(size),
        Utils::to_vec2(origin),
        min_depth,
        max_depth
    );
}

void GraphicsPipeline::set_y_up_viewport(
    VulkanCommandBuffer& command_buffer, 
    glm::vec2 size,
    glm::vec2 origin,
    float min_depth,
    float max_depth)
{
    set_y_down_viewport(
        command_buffer,
        glm::vec2{size.x, -size.y},
        glm::vec2{origin.x, origin.y + size.y},
        min_depth,
        max_depth
    );
}

void GraphicsPipeline::set_y_up_viewport(
    VulkanCommandBuffer& command_buffer, 
    VkExtent2D size,
    VkOffset2D origin,
    float min_depth,
    float max_depth)
{
    set_y_up_viewport(
        command_buffer,
        Utils::to_vec2(size),
        Utils::to_vec2(origin),
        min_depth,
        max_depth
    );
}

void GraphicsPipeline::set_scissor(
    VulkanCommandBuffer& command_buffer,
    VkExtent2D extent,
    VkOffset2D offset)
{
    LOG_NAMED("GraphicsPipeline");

    logger.check(command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initialized");

    VkRect2D scissor{};
    scissor.offset = offset;
    scissor.extent = extent;

    vkCmdSetScissor(command_buffer.handle(), 0, 1, &scissor);
}

void GraphicsPipeline::set_y_down_viewport(
    VulkanCommandBuffer& command_buffer,
    VulkanEngine& engine,
    VkOffset2D origin,
    float min_depth,
    float max_depth)
{
    set_y_down_viewport(
        command_buffer,
        engine.swapchain_resources().swapchain.extent(),
        origin,
        min_depth,
        max_depth
    );
}

void GraphicsPipeline::set_y_up_viewport(
    VulkanCommandBuffer& command_buffer,
    VulkanEngine& engine,
    VkOffset2D origin,
    float min_depth,
    float max_depth)
{
    set_y_up_viewport(
        command_buffer,
        engine.swapchain_resources().swapchain.extent(),
        origin,
        min_depth,
        max_depth
    );
}

void GraphicsPipeline::set_scissor(
    VulkanCommandBuffer& command_buffer,
    VulkanEngine& engine,
    VkOffset2D offset)
{
    set_scissor(
        command_buffer,
        engine.swapchain_resources().swapchain.extent(),
        offset
    );
}