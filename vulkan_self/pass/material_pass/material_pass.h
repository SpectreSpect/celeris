#pragma once

#include <cstdint>

#include "../../logger/logger_header.h"
#include "../pipeline_pass.h"
#include "../../pipeline/graphics_pipeline/graphics_pipeline.h"

class VulkanPipelineLayout;
class DescriptorSetLayout;
class MaterialPassBuilder;
class VulkanRenderPass;
class VulkanEngine;
class VulkanDevice;
class VulkanCommandBuffer;
class Pipeline;

class MaterialPass : public PipelinePass {
public:
    _XCHILD_NAME(MaterialPass);

    explicit MaterialPass(
        DescriptorSetLayout&& descriptor_set_layout,
        VulkanPipelineLayout&& pipeline_layout,
        GraphicsPipeline&& pipeline,
        uint32_t material_set_index = 0
    );
    
    explicit MaterialPass(VulkanDevice& device, VulkanRenderPass& render_pass, MaterialPassBuilder& builder);
    explicit MaterialPass(VulkanEngine& engine, MaterialPassBuilder& builder);

    MaterialPass(const MaterialPass&) = delete;
    MaterialPass& operator=(const MaterialPass&) = delete;

    MaterialPass(MaterialPass&&) noexcept = default;
    MaterialPass& operator=(MaterialPass&&) noexcept = default;

    GraphicsPipeline& pipeline() noexcept;

    virtual VkPipelineBindPoint bind_point() const noexcept override;
    virtual const Pipeline& pipeline() const override;
    virtual void bind(VulkanCommandBuffer& command_buffer) const override;

private:
    struct BuildData {
        DescriptorSetLayout descriptor_set_layout;
        VulkanPipelineLayout pipeline_layout;
        GraphicsPipeline pipeline;
    };

    static BuildData build(
        VulkanDevice& device,
        VulkanRenderPass& render_pass,
        MaterialPassBuilder& builder
    );
    
    explicit MaterialPass(BuildData data);

private:
    GraphicsPipeline m_pipeline;
};
