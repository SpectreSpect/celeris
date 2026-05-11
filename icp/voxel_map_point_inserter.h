#pragma once

#include "../vulkan_engine.h"
#include "../vulkan/compute_pipeline.h"
#include "../vulkan/fence.h"
#include "../vulkan/image/texture2d.h"
#include "../vulkan/image/cubemap.h"
#include "../vulkan/graphics_pipeline.h"
#include "../vulkan/descriptor_set_bundle.h"
#include "../vulkan/render_target_2d.h"
#include "../vulkan/command_pool.h"
#include "../vulkan/command_buffer.h"
#include "../point_cloud/point_cloud.h"
#include "voxel_point_map.h"


class VoxelMapPointInserter {
public:
    // struct InserterUniform {
    //     uint32_t source_point_count;
    //     uint32_t max_map_point_count;
    //     uint32_t num_hash_table_slots;
    //     uint32_t _pad0;
    //     alignas(16) glm::mat4 source_model;
    //     glm::vec4 color;
    // };


    struct InserterUniform {
        uint32_t source_point_count;
        uint32_t max_map_point_count;
        uint32_t num_hash_table_slots;
        uint32_t _pad0;

        glm::mat4 source_model;
        glm::vec4 color;
    };

    static_assert(offsetof(InserterUniform, source_model) == 16);
    static_assert(offsetof(InserterUniform, color) == 80);

    // static_assert(sizeof(InserterUniform) == 80);

    VoxelMapPointInserter() = default;
    void create(VulkanEngine& engine);
    void insert(VoxelPointMap& voxel_point_map, PointCloud& source_point_cloud, VideoBuffer& source_normal_buffer);
    
private:
    VulkanEngine* engine = nullptr;
    ComputePipeline pipeline;
    DescriptorSetBundle descriptor_set_bundle;
    Fence fence;
    VideoBuffer uniform_buffer;
    ShaderModule shader_module;
    
    uint32_t compute_queue_family_id;
    VkQueue compute_queue;

    CommandPool command_pool;
    CommandBuffer command_buffer;
};