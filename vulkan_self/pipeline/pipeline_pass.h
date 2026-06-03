#pragma once

#include <vulkan/vulkan.h>

#include "../logger/logger_header.h"
#include "../descriptor_set/descriptor_set_layout.h"
#include "vulkan_pipeline_layout.h"

class VulkanCommandBuffer;
class Pipeline;

class PipelinePass {
public:
    _XPARENT_NAME(PipelinePass);

    virtual ~PipelinePass() noexcept = default;

    PipelinePass(const PipelinePass&) = delete;
    PipelinePass& operator=(const PipelinePass&) = delete;

    PipelinePass(PipelinePass&&) noexcept = default;
    PipelinePass& operator=(PipelinePass&&) noexcept = default;

    const DescriptorSetLayout& descriptor_set_layout() const noexcept;
    const VulkanPipelineLayout& pipeline_layout() const noexcept;
    uint32_t descriptor_set_index() const noexcept;

    virtual VkPipelineBindPoint bind_point() const noexcept = 0;
    virtual const Pipeline& pipeline() const = 0;
    Pipeline& pipeline();
    virtual void bind(VulkanCommandBuffer& command_buffer) const = 0;

protected:
    PipelinePass(
        DescriptorSetLayout&& descriptor_set_layout,
        VulkanPipelineLayout&& pipeline_layout,
        uint32_t descriptor_set_index = 0
    );

private:
    DescriptorSetLayout m_descriptor_set_layout;
    VulkanPipelineLayout m_pipeline_layout;

    uint32_t m_descriptor_set_index = 0;
};
