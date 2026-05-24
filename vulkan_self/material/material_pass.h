#pragma once

#include <cstdint>

#include "../logger/logger_header.h"
#include "../pipeline/pipeline_pass.h"
#include "../pipeline/graphics_pipeline/graphics_pipeline.h"

class VulkanPipelineLayout;
class DescriptorSetLayout;
class MaterialPassBuilder;
class VulkanRenderPass;
class VulkanEngine;
class VulkanDevice;

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

// #pragma once

// #include "../logger/logger_header.h"
// #include "../descriptor_set/descriptor_set_layout.h"
// #include "../pipeline/vulkan_pipeline_layout.h"
// #include "../pipeline/graphics_pipeline.h"

// class MaterialPass {
// public:
//     _XCLASS_NAME(MaterialPass);

//     MaterialPass(VulkanDevice& device, DescriptorSetLayoutBuilder dls_builder, 
//                  PipelineLayoutBuilder pipeline_layout_builder, GraphicsPipelineBuilder pipeline_builder) 
//         :   m_descriptor_set_layout(device, dls_builder),
//             m_pipeline_layout(pipeline_layout_builder),
//             m_pipeline(pipeline_builder) {};
    
//     DescriptorSetLayout m_descriptor_set_layout;
//     VulkanPipelineLayout m_pipeline_layout;
//     GraphicsPipeline m_pipeline;

//     uint32_t m_material_set_index = 0;
// };