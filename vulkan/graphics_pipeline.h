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

class VulkanEngine;

class GraphicsPipeline : public Pipeline {
public:
    
    
    // VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    // VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    // VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    
    
    // DescriptorSetLayout descriptor_set_layout;
    // DescriptorPool descriptor_pool;
    // DescriptorSet descriptor_set;

    // VideoBuffer uniform_buffer;

    GraphicsPipeline() = default;
    GraphicsPipeline(VulkanEngine& engine, DescriptorSetBundle& descriptor_set_bundle, VulkanVertexLayout& vertex_layout, VulkanShaderModule& vert_module, VulkanShaderModule& frag_module, bool depth_test = true);
    void create(VulkanEngine& engine, RenderPass& render_pass, VkExtent2D& extent, DescriptorSetBundle& descriptor_set_bundle,
            VulkanVertexLayout& vertex_layout, VkShaderModule& vert_module, VkShaderModule& frag_module, bool depth_test = true);

    void create(VulkanEngine& engine, RenderPass& render_pass, VkExtent2D& extent,
                DescriptorSetBundle& descriptor_set_bundle,
                VulkanVertexLayout& vertex_layout, VulkanShaderModule& vert_module, VulkanShaderModule& frag_module, bool depth_test = true);

    void create(VkDevice& device, VkPhysicalDevice& physical_device, VkRenderPass& render_pass, 
                VkExtent2D& swapchain_extent, DescriptorSetBundle& descriptor_set_bundle, VulkanVertexLayout& vertex_layout, 
                VkShaderModule& vert_module, VkShaderModule& frag_module, bool depth_test = true);
    void create(VulkanEngine& engine, DescriptorSetBundle& descriptor_set_bundle, VulkanVertexLayout& vertex_layout, VkShaderModule& vert_module, VkShaderModule& frag_module, bool depth_test = true);
    void create(VulkanEngine& engine, DescriptorSetBundle& descriptor_set_bundle, VulkanVertexLayout& vertex_layout, VulkanShaderModule& vert_module, VulkanShaderModule& frag_module, bool depth_test = true);
    
    VkPipelineBindPoint get_bind_point() const override {
        return VK_PIPELINE_BIND_POINT_GRAPHICS;
    };
};