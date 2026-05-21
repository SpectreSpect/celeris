#pragma once

#include "../../vulkan_self/logger/logger_header.h"
#include "../../vulkan_self/descriptor_set/descriptor_set.h"
#include "../../vulkan_self/descriptor_set/descriptor_set_layout.h"
#include "../../vulkan_self/descriptor_set/descriptor_set_layout_builder.h"
#include "../../vulkan_self/descriptor_set/descriptor_pool_builder.h"
#include "../../vulkan_self/descriptor_set/descriptor_pool.h"
#include "../../vulkan_self/vulkan_buffer.h"
#include "../../vulkan_self/vulkan_engine.h"
#include "../../vulkan_self/pipeline/pipeline.h"
#include "../../camera/camera.h"


class FrameResources {
public:
    _XCLASS_NAME(FrameResources);    
    
    struct CameraUniform {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec4 view_pos;
    };

    FrameResources(VulkanPhysicalDevice& physical_device, VulkanDevice& device, uint32_t num_frames_in_flight);

    DescriptorSetLayout& descriptor_layout() noexcept;

    void update_camera(uint32_t frame_id, const Camera& camera);
    void bind(uint32_t frame_id, VulkanCommandBuffer& command_buffer, Pipeline& pipeline, uint32_t set_binding);

private:
    DescriptorSetLayoutBuilder descriptor_layout_builder;
    DescriptorSetLayout m_descriptor_layout;

    DescriptorPool descriptor_pool;
    
    std::vector<DescriptorSet> descriptor_sets;
    std::vector<VulkanBuffer> camera_uniforms;

    DescriptorSetLayoutBuilder create_dsl_builder();
    DescriptorPool create_descriptor_pool(VulkanDevice& device, uint32_t num_frames_in_flight);
};