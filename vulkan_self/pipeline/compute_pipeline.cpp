#include "compute_pipeline.h"

#include <vulkan/vk_enum_string_helper.h>

#include "vulkan_pipeline_layout.h"
#include "../vulkan_device.h"
#include "../vulkan_shader_module.h"

ComputePipeline::ComputePipeline(VulkanDevice& device, const VulkanPipelineLayout& pipeline_layout, VulkanShaderModule& compute_shader) {
    LOG_METHOD();

    logger.check(compute_shader.handle() != VK_NULL_HANDLE, "compute_shader was null");
    logger.check(pipeline_layout.handle() != VK_NULL_HANDLE, "pipeline_layout was null");
    logger.check(device.handle() != VK_NULL_HANDLE, "device was null");

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = compute_shader.handle();
    shaderStageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = shaderStageInfo;
    pipelineInfo.layout = pipeline_layout.handle();

    VkPipeline pipeline = VK_NULL_HANDLE;

    VkResult result = vkCreateComputePipelines(device.handle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    logger.check(result == VK_SUCCESS) << "Failed to create compute pipeline: " << clr(string_VkResult(result), LoggerPalette::blue) << "\n";

    set_pipeline(device.handle(), pipeline, pipeline_layout.handle());
}

VkPipelineBindPoint ComputePipeline::get_bind_point() const noexcept {
    return VK_PIPELINE_BIND_POINT_COMPUTE;
}

// void ComputePipeline::bind(VulkanCommandBuffer& command_buffer) const {
//     LOG_METHOD();

//     logger.check(m_pipeline != VK_NULL_HANDLE, "Pipeline is not initialized");
//     logger.check(command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initialized");

//     vkCmdBindPipeline(command_buffer.handle(), get_bind_point(), m_pipeline);
// }