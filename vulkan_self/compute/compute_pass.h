#pragma once

#include <cstdint>

#include "../pipeline/pipeline_pass.h"
#include "../pipeline/compute_pipeline/compute_pipeline.h"

class DescriptorSetLayout;
class VulkanPipelineLayout;
class ComputePassBuilder;
class VulkanDevice;
class VulkanCommandBuffer;
class Pipeline;

class ComputePass : public PipelinePass {
public:
    _XCHILD_NAME(ComputePass);

    explicit ComputePass(
        DescriptorSetLayout&& descriptor_set_layout,
        VulkanPipelineLayout&& pipeline_layout,
        ComputePipeline&& pipeline
    );

    explicit ComputePass(VulkanDevice& device, ComputePassBuilder& builder);

    virtual ~ComputePass() noexcept override = default;

    ComputePass(const ComputePass&) = delete;
    ComputePass& operator=(const ComputePass&) = delete;

    ComputePass(ComputePass&&) noexcept = default;
    ComputePass& operator=(ComputePass&&) noexcept = default;

    ComputePipeline& compute_pipeline() noexcept;
    const ComputePipeline& compute_pipeline() const noexcept;

    virtual VkPipelineBindPoint bind_point() const noexcept override;
    virtual const Pipeline& pipeline() const override;
    virtual void bind(VulkanCommandBuffer& command_buffer) const override;

private:
    ComputePipeline m_pipeline;

private:
    struct BuildData {
        DescriptorSetLayout descriptor_set_layout;
        VulkanPipelineLayout pipeline_layout;
        ComputePipeline pipeline;
    };

    static BuildData build(VulkanDevice& device, ComputePassBuilder& builder);

    explicit ComputePass(BuildData data);
};
