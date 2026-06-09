#pragma once
#include "pipeline.h"
#include "vulkan_vertex_layout.h"
#include "video_buffer.h"
#include "shader_module.h"
#include "video_buffer.h"
#include "descriptor_set_layout.h"
#include "descriptor_pool.h"
#include "descriptor_set.h"
#include "descriptor_set_bundle.h"
#include "render_pass.h"
#include "shader_module.h"

class VulkanEngine;

class ComputePipeline : public Pipeline {
public:
    ComputePipeline() = default;
    ComputePipeline(VkDevice& device, DescriptorSetBundle& descriptor_set_bundle, VulkanShaderModule& compute_shader);
    void create(VkDevice& device, DescriptorSetBundle& descriptor_set_bundle, VulkanShaderModule& compute_shader);
    VkPipelineBindPoint get_bind_point() const override {
        return VK_PIPELINE_BIND_POINT_COMPUTE;
    };

    // void create(VulkanEngine& engine, RenderPass& render_pass, VkExtent2D& extent, DescriptorSetBundle& descriptor_set_bundle,
    //         VulkanVertexLayout& vertex_layout, VkShaderModule& vert_module, VkShaderModule& frag_module);

    // void create(VulkanEngine& engine, RenderPass& render_pass, VkExtent2D& extent,
    //             DescriptorSetBundle& descriptor_set_bundle,
    //             VulkanVertexLayout& vertex_layout, ShaderModule& vert_module, ShaderModule& frag_module);

    // void create(VkDevice& device, VkPhysicalDevice& physical_device, VkRenderPass& render_pass, 
    //             VkExtent2D& swapchain_extent, DescriptorSetBundle& descriptor_set_bundle, VulkanVertexLayout& vertex_layout, 
    //             VkShaderModule& vert_module, VkShaderModule& frag_module);
    // void create(VulkanEngine& engine, DescriptorSetBundle& descriptor_set_bundle, VulkanVertexLayout& vertex_layout, VkShaderModule& vert_module, VkShaderModule& frag_module);
    // void create(VulkanEngine& engine, DescriptorSetBundle& descriptor_set_bundle, VulkanVertexLayout& vertex_layout, ShaderModule& vert_module, ShaderModule& frag_module);
};