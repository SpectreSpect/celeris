#include "graphics_pipeline.h"
#include "../vulkan_engine.h"

GraphicsPipeline::GraphicsPipeline(VulkanEngine& engine, DescriptorSetBundle& descriptor_set_bundle,
                                   VulkanVertexLayout& vertex_layout, VulkanShaderModule& vert_module, VulkanShaderModule& frag_module, bool depth_test) {
    create(engine,  descriptor_set_bundle, vertex_layout, vert_module, frag_module, depth_test);
}

void GraphicsPipeline::create(VulkanEngine& engine, RenderPass& render_pass, VkExtent2D& extent, DescriptorSetBundle& descriptor_set_bundle,
                              VulkanVertexLayout& vertex_layout, VkShaderModule& vert_module, VkShaderModule& frag_module, bool depth_test) {
    create(engine.device,
           engine.physicalDevice,
           render_pass.render_pass,
           extent,
           descriptor_set_bundle,
           vertex_layout,
           vert_module,
           frag_module, depth_test);
}

void GraphicsPipeline::create(VulkanEngine& engine, RenderPass& render_pass, VkExtent2D& extent, DescriptorSetBundle& descriptor_set_bundle,
                              VulkanVertexLayout& vertex_layout, VulkanShaderModule& vert_module, VulkanShaderModule& frag_module, bool depth_test) {
    create(engine,
           render_pass,
           extent,
           descriptor_set_bundle,
           vertex_layout,
           vert_module.shader_module,
           frag_module.shader_module, depth_test);
}
void GraphicsPipeline::create(VkDevice& device, VkPhysicalDevice& physical_device, VkRenderPass& render_pass, 
                              VkExtent2D& swapchain_extent,
                              DescriptorSetBundle& descriptor_set_bundle, VulkanVertexLayout& vertex_layout, VkShaderModule& vert_module, VkShaderModule& frag_module, bool depth_test){
        this->descriptor_set_bundle = &descriptor_set_bundle;

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vert_module;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = frag_module;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertStage, fragStage };

    VkPipelineVertexInputStateCreateInfo vertexInput = vertex_layout.get_vertex_intput();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // No baked-in viewport/scissor anymore
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptor_set_bundle.descriptor_set_layout.layout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    VulkanEngine::vk_check(
        vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipeline_layout),
        "vkCreatePipelineLayout"
    );

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipeline_layout;
    pipelineInfo.renderPass = render_pass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VulkanEngine::vk_check(
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline),
        "vkCreateGraphicsPipelines"
    );
}

void GraphicsPipeline::create(VulkanEngine& engine, DescriptorSetBundle& descriptor_set_bundle, VulkanVertexLayout& vertex_layout, 
                              VkShaderModule& vert_module, VkShaderModule& frag_module, bool depth_test) {
    create(engine.device, engine.physicalDevice, engine.renderPass, engine.swapchainExtent,
        descriptor_set_bundle, vertex_layout, vert_module, frag_module, depth_test);
}

void GraphicsPipeline::create(VulkanEngine& engine, DescriptorSetBundle& descriptor_set_bundle, VulkanVertexLayout& vertex_layout, 
                              VulkanShaderModule& vert_module, VulkanShaderModule& frag_module, bool depth_test) {
    create(engine, descriptor_set_bundle, vertex_layout, vert_module.shader_module, frag_module.shader_module, depth_test);
}