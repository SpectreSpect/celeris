#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

#include "../logger/logger_header.h"
#include "../descriptor_set/descriptor_set_layout_builder.h"
#include "vulkan_pipeline_layout.h"

class DescriptorSetLayout;
class VulkanPipelineLayout;
class VulkanDevice;

class PipelinePassBuilder {
public:
    _XPARENT_NAME(PipelinePassBuilder);

    PipelinePassBuilder() = default;

    void add_uniform_buffer(uint32_t binding, VkShaderStageFlags shader_stage_flags);
    void add_storage_buffer(uint32_t binding, VkShaderStageFlags shader_stage_flags);
    void add_combined_image_sampler(uint32_t binding, VkShaderStageFlags shader_stage_flags);
    void add_push_constants(uint32_t size_bytes, uint32_t offset = 0, VkShaderStageFlags stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    void add_push_constantsf(uint32_t size_bytes, VkShaderStageFlags stage_flags);
    void add_descriptor_set_layout(const DescriptorSetLayout& layout);

    const DescriptorSetLayoutBuilder& material_dsl_builder() const;
    const PipelineLayoutBuilder& pipeline_layout_builder() const;

    DescriptorSetLayout create_descriptor_set_layout(VulkanDevice& device);
    VulkanPipelineLayout create_pipeline_layout(VulkanDevice& device, const DescriptorSetLayout& material_ds_layout);

private:
    DescriptorSetLayoutBuilder m_material_dsl_builder;
    PipelineLayoutBuilder m_pipeline_layout_builder;
};
