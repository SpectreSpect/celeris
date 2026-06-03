#include "compute_pass.h"

#include <utility>

#include "../descriptor_set/descriptor_set_layout.h"
#include "../pipeline/vulkan_pipeline_layout.h"
#include "../compute/compute_pass_builder.h"
#include "../vulkan_device.h"
#include "../vulkan_command_buffer.h"
#include "../pipeline/pipeline.h"

ComputePass::ComputePass(
    DescriptorSetLayout&& descriptor_set_layout,
    VulkanPipelineLayout&& pipeline_layout,
    ComputePipeline&& pipeline)
    :   PipelinePass(
        std::move(descriptor_set_layout),
        std::move(pipeline_layout)
    ),
    m_pipeline(std::move(pipeline)) {}

ComputePass::ComputePass(BuildData data)
    :   ComputePass(
            std::move(data.descriptor_set_layout),
            std::move(data.pipeline_layout),
            std::move(data.pipeline)
        ) {}

ComputePass::ComputePass(VulkanDevice& device, ComputePassBuilder& builder)
    :   ComputePass(build(device, builder)) {}


ComputePass::BuildData ComputePass::build(VulkanDevice& device, ComputePassBuilder& builder) {
    DescriptorSetLayout descriptor_set_layout(builder.create_descriptor_set_layout(device));
    VulkanPipelineLayout pipeline_layout(builder.create_pipeline_layout(device, descriptor_set_layout));
    ComputePipeline pipeline(builder.create_compute_pipeline(device, pipeline_layout));

    return BuildData{
        .descriptor_set_layout = std::move(descriptor_set_layout),
        .pipeline_layout = std::move(pipeline_layout),
        .pipeline = std::move(pipeline)
    };
}

ComputePipeline& ComputePass::compute_pipeline() noexcept {
    return m_pipeline;
}

const ComputePipeline& ComputePass::compute_pipeline() const noexcept {
    return m_pipeline;
}

VkPipelineBindPoint ComputePass::bind_point() const noexcept {
    return m_pipeline.get_bind_point();
}

const Pipeline& ComputePass::pipeline() const {
    LOG_METHOD();

    logger.check(m_pipeline.handle() != VK_NULL_HANDLE, "Pipline is not initialized");

    return m_pipeline;
}

void ComputePass::bind(VulkanCommandBuffer& command_buffer) const {
    LOG_METHOD();

    logger.check(command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initialized");

    m_pipeline.bind(command_buffer);
}
