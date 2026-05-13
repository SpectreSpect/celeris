#include "compute_pipeline.h"


ComputePipeline::ComputePipeline(VkDevice& device, DescriptorSetBundle& descriptor_set_bundle, VulkanShaderModule& compute_shader) {
    create(device, descriptor_set_bundle, compute_shader);
}

void ComputePipeline::create(VkDevice& device, DescriptorSetBundle& descriptor_set_bundle, VulkanShaderModule& compute_shader) {
    this->descriptor_set_bundle = &descriptor_set_bundle;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptor_set_bundle.descriptor_set_layout.layout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &this->pipeline_layout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout");
    }

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = compute_shader.shader_module;
    shaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = this->pipeline_layout;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &this->pipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline");
    }
}