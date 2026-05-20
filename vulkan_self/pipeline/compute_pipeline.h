#pragma once

#include <vulkan/vulkan.h>

#include "../logger/logger_header.h"
#include "pipeline.h"

class VulkanDevice;
class VulkanShaderModule;
class VulkanPipelineLayout;
class VulkanCommandBuffer;

class ComputePipeline : public Pipeline {
public:
    _XCHILD_NAME(ComputePipeline);
    
    explicit ComputePipeline(VulkanDevice& device, const VulkanPipelineLayout& pipeline_layout, VulkanShaderModule& compute_shader);
    
    VkPipelineBindPoint get_bind_point() const noexcept override;
};