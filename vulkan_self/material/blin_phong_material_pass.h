// #pragma once

// #include "../logger/logger_header.h"
// #include "../descriptor_set/descriptor_set_layout.h"
// #include "../pipeline/vulkan_pipeline_layout.h"
// #include "../pipeline/graphics_pipeline.h"
// #include "../descriptor_set/descriptor_set_layout_builder.h"
// #include "../vulkan_engine.h"
// #include "../../renderer/transform_push_constants.h"
// #include "../vulkan_shader_module.h"
// #include "../vulkan_device.h"

// #include "material_pass.h"

// class BlinPhongMaterialPass : public MaterialPass {
// public:
//     _XCLASS_NAME(BlinPhongMaterialPass);

//     explicit BlinPhongMaterialPass(VulkanEngine& engine, DescriptorSetLayout& frame_resources_descriptor_layout, VulkanShaderModule& vertex_shader, VulkanShaderModule& fragment_shader);

//     DescriptorSetLayoutBuilder create_dls_builder();
//     PipelineLayoutBuilder create_pipeline_layout_builder(VulkanDevice& device, DescriptorSetLayout& frame_resources_descriptor_layout);
//     GraphicsPipelineBuilder create_pipeline_builder(VulkanEngine& engine, VulkanShaderModule& vertex_shader, VulkanShaderModule& fragment_shader);
// };