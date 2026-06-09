#include "compute_pass_builder.h"

#include "../../pipeline/compute_pipeline/compute_pipeline.h"
#include "../../vulkan_device.h"
#include "../../pipeline/vulkan_pipeline_layout.h"
#include "../../vulkan_shader_module.h"

ComputePassBuilder& ComputePassBuilder::set_compute_shader(
    VulkanShaderModule& compute_shader_module, 
    std::string_view entry_point_name)
{
    LOG_METHOD();

    m_compute_shader_module = &compute_shader_module;
    m_compute_shader_entry_point = std::string(entry_point_name);

    return *this;
}

ComputePipeline ComputePassBuilder::create_compute_pipeline(
    VulkanDevice& device, 
    const VulkanPipelineLayout& pipeline_layout) 
{
    LOG_METHOD();

    logger.check(m_compute_shader_module != nullptr, "Pointer to compute shader module specify to null");

    return ComputePipeline(device, pipeline_layout, *m_compute_shader_module, m_compute_shader_entry_point);
}
