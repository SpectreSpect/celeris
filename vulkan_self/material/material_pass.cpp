#include "material_pass.h"

#include <utility>

#include "../pipeline/vulkan_pipeline_layout.h"
#include "../descriptor_set/descriptor_set_layout.h"
#include "material_pass_builder.h"
#include "../vulkan_render_pass.h"
#include "../vulkan_engine.h"
#include "../vulkan_device.h"
#include "../vulkan_command_buffer.h"
#include "../pipeline/pipeline.h"

MaterialPass::MaterialPass(
    DescriptorSetLayout&& descriptor_set_layout,
    VulkanPipelineLayout&& pipeline_layout,
    GraphicsPipeline&& pipeline,
    uint32_t material_set_index)
    :   PipelinePass(
            std::move(descriptor_set_layout),
            std::move(pipeline_layout),
            material_set_index
        ),
        m_pipeline(std::move(pipeline)) {}

MaterialPass::MaterialPass(
    VulkanDevice& device,
    VulkanRenderPass& render_pass,
    MaterialPassBuilder& builder)
    :   MaterialPass(build(device, render_pass, builder)) {}

MaterialPass::MaterialPass(BuildData data)
    :   PipelinePass(
            std::move(data.descriptor_set_layout),
            std::move(data.pipeline_layout),
            0
        ),
        m_pipeline(std::move(data.pipeline)) {}

MaterialPass::BuildData MaterialPass::build(
    VulkanDevice& device,
    VulkanRenderPass& render_pass,
    MaterialPassBuilder& builder)
{
    DescriptorSetLayout dsl = builder.create_descriptor_set_layout(device);
    VulkanPipelineLayout pipeline_layout = builder.create_pipeline_layout(device, dsl);
    GraphicsPipeline graphics_pipeline = builder.create_graphics_pipeline(device, render_pass, pipeline_layout);
    
    return BuildData {
        .descriptor_set_layout = std::move(dsl),
        .pipeline_layout = std::move(pipeline_layout),
        .pipeline = std::move(graphics_pipeline)
    };
}

MaterialPass::MaterialPass(VulkanEngine& engine, MaterialPassBuilder& builder)
    :   MaterialPass(engine.device(), engine.swapchain_resources().render_pass, builder) {}

GraphicsPipeline& MaterialPass::pipeline() noexcept {
    return m_pipeline;
}

VkPipelineBindPoint MaterialPass::bind_point() const noexcept {
    return m_pipeline.get_bind_point();
}

const Pipeline& MaterialPass::pipeline() const {
    LOG_METHOD();

    logger.check(m_pipeline.handle() != VK_NULL_HANDLE, "Pipline is not initialized");

    return m_pipeline;
}

void MaterialPass::bind(VulkanCommandBuffer& command_buffer) const {
    LOG_METHOD();

    logger.check(command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initialized");

    m_pipeline.bind(command_buffer);
}
