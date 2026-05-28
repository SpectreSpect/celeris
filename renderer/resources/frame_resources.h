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


class LightingSystem;

class FrameResources {
public:
    _XCLASS_NAME(FrameResources);    
    
    struct FrameData {
        DescriptorSet descriptor_set;

        VulkanBuffer camera_uniform;

        VulkanBuffer light_source_ssbo;
        VulkanBuffer lights_in_clusters_ssbo;
        VulkanBuffer num_lights_in_clusters_ssbo;
        VulkanBuffer cluster_aabbs_ssbo;
        VulkanBuffer lighting_system_uniform;

        FrameData(
            VulkanEngine& engine,
            DescriptorPool& descriptor_pool,
            DescriptorSetLayout& descriptor_layout,
            VkDeviceSize camera_uniform_size,
            VkDeviceSize light_sources_size,
            VkDeviceSize lights_in_clusters_size,
            VkDeviceSize num_lights_in_clusters_size,
            VkDeviceSize cluster_aabbs_size,
            VkDeviceSize lighting_uniform_size
        )
            : descriptor_set(descriptor_pool.allocate_set(descriptor_layout)),
            camera_uniform(
                VulkanBuffer::create_host_visible_uniform_buffer(
                    engine,
                    camera_uniform_size
                )
            ),
            light_source_ssbo(
                VulkanBuffer::create_host_visible_storage_buffer(
                    engine,
                    light_sources_size
                )
            ),
            lights_in_clusters_ssbo(
                VulkanBuffer::create_host_visible_transfer_dst_storage_buffer(
                    engine,
                    lights_in_clusters_size
                )
            ),
            num_lights_in_clusters_ssbo(
                VulkanBuffer::create_host_visible_transfer_dst_storage_buffer(
                    engine,
                    num_lights_in_clusters_size
                )
            ),
            cluster_aabbs_ssbo(
                VulkanBuffer::create_host_visible_storage_buffer(
                    engine,
                    cluster_aabbs_size
                )
            ),
            lighting_system_uniform(
                VulkanBuffer::create_host_visible_uniform_buffer(
                    engine,
                    lighting_uniform_size
                )
            )
        {}
    };
    
    struct CameraUniform {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec4 view_pos;
        glm::vec2 viewport;
    };

    FrameResources(VulkanEngine& engine, LightingSystem& lighting_system, uint32_t num_frames_in_flight);

    DescriptorSetLayout& descriptor_layout() noexcept;

    void update_camera(uint32_t frame_id, const Camera& camera);
    void bind(uint32_t frame_id, VulkanCommandBuffer& command_buffer, Pipeline& pipeline, uint32_t set_binding);
    FrameData& frame_data(uint32_t frame_id);

private:
    DescriptorSetLayoutBuilder descriptor_layout_builder;
    DescriptorSetLayout m_descriptor_layout;

    DescriptorPool descriptor_pool;
    
    std::vector<DescriptorSet> descriptor_sets;
    std::vector<VulkanBuffer> camera_uniforms;

    std::vector<FrameData> m_frame_data;

    DescriptorSetLayoutBuilder create_dsl_builder();
    DescriptorPool create_descriptor_pool(VulkanDevice& device, uint32_t num_frames_in_flight);
};