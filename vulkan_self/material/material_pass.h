#pragma once

#include "../logger/logger_header.h"
#include "../descriptor_set/descriptor_set_layout.h"
#include "../pipeline/vulkan_pipeline_layout.h"
#include "../pipeline/graphics_pipeline.h"
#include "material_pass_builder.h"
#include "../vulkan_engine.h"

class MaterialPass {
public:
    _XCLASS_NAME(MaterialPass);

    explicit MaterialPass(
        DescriptorSetLayout&& descriptor_set_layout,
        VulkanPipelineLayout&& pipeline_layout,
        GraphicsPipeline&& pipeline,
        uint32_t material_set_index = 0
    )
        :   m_descriptor_set_layout(std::move(descriptor_set_layout)),
            m_pipeline_layout(std::move(pipeline_layout)),
            m_pipeline(std::move(pipeline)),
            m_material_set_index(material_set_index) {}
    
    explicit MaterialPass(VulkanDevice& device, VulkanRenderPass& render_pass, MaterialPassBuilder& builder);
    explicit MaterialPass(VulkanEngine& engine, MaterialPassBuilder& builder);

    MaterialPass(const MaterialPass&) = delete;
    MaterialPass& operator=(const MaterialPass&) = delete;

    MaterialPass(MaterialPass&&) noexcept = default;
    MaterialPass& operator=(MaterialPass&&) noexcept = default;

    const DescriptorSetLayout& descriptor_set_layout() const noexcept {
        return m_descriptor_set_layout;
    }

    const VulkanPipelineLayout& pipeline_layout() const noexcept {
        return m_pipeline_layout;
    }

    GraphicsPipeline& pipeline() noexcept {
        return m_pipeline;
    }

    uint32_t material_set_index() const noexcept {
        return m_material_set_index;
    }

private:
    DescriptorSetLayout m_descriptor_set_layout;
    VulkanPipelineLayout m_pipeline_layout;
    GraphicsPipeline m_pipeline;

    uint32_t m_material_set_index = 0;
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