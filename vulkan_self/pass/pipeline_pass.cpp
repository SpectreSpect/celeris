#include "pipeline_pass.h"

#include <utility>

PipelinePass::PipelinePass(
    DescriptorSetLayout&& descriptor_set_layout,
    VulkanPipelineLayout&& pipeline_layout,
    uint32_t descriptor_set_index)
    :   m_descriptor_set_layout(std::move(descriptor_set_layout)),
        m_pipeline_layout(std::move(pipeline_layout)),
        m_descriptor_set_index(descriptor_set_index) {}

const DescriptorSetLayout& PipelinePass::descriptor_set_layout() const noexcept {
    return m_descriptor_set_layout;
}

const VulkanPipelineLayout& PipelinePass::pipeline_layout() const noexcept {
    return m_pipeline_layout;
}

uint32_t PipelinePass::descriptor_set_index() const noexcept {
    return m_descriptor_set_index;
}

Pipeline& PipelinePass::pipeline() {
    LOG_METHOD();

    return const_cast<Pipeline&>(
        std::as_const(*this).pipeline()
    );
}
