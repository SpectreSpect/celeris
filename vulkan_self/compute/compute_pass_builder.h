#pragma once

#include <vulkan/vulkan.h>
#include <string_view>
#include <string>

#include "../logger/logger_header.h"
#include "../pipeline/pipeline_pass_builder.h"

class ComputePipeline;
class VulkanDevice;
class VulkanPipelineLayout;
class VulkanShaderModule;

class ComputePassBuilder : public PipelinePassBuilder {
public:
    _XCHILD_NAME(ComputePassBuilder)

    ComputePassBuilder() =  default;

    ComputePassBuilder& set_compute_shader(
        VulkanShaderModule& compute_shader_module, 
        std::string_view entry_point_name = "main"
    );

    ComputePipeline create_compute_pipeline(
        VulkanDevice& device, 
        const VulkanPipelineLayout& pipeline_layout
    );
private:
    VulkanShaderModule* m_compute_shader_module = nullptr;
    std::string m_compute_shader_entry_point = "main";
};
