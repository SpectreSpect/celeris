#pragma once

#include "../../../../vulkan_self/pass/instance/pass_instance.h"
#include "../../../../vulkan_self/vulkan_buffer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

class InstanceBufferView;
class VoxelGrid;

class OldVoxelPointMap {
public:
    struct VoxelPointMapUniform {
        uint32_t source_point_count;
        uint32_t max_map_point_count;
    };

    struct HashTableCountersGpu {
        uint32_t count_empty[16];
        uint32_t count_occupied[16];
        uint32_t count_tomb[16];
    };

    struct IndexHashTableSlotGpu {
        glm::uvec2 key;
        uint32_t value;
        uint32_t state;
    };

    static_assert(sizeof(HashTableCountersGpu) == 192);
    static_assert(sizeof(IndexHashTableSlotGpu) == 16);

    OldVoxelPointMap(VulkanEngine& engine, uint32_t num_hash_table_slots, uint32_t max_map_point_count);

    InstanceBufferView get_map_instance_view();

    void upload_voxels(VulkanEngine& engine, VoxelGrid& voxel_grid);

    // OldVoxelPointMap() = default;
    // void create(VulkanBuffer& engine, uint32_t num_hash_table_slots, uint32_t max_map_point_count);
    // void get_point_cloud_frame(PointCloudFrame* frame);

    uint32_t m_num_hash_table_slots = 0;
    uint32_t m_max_map_point_count = 0;
    uint32_t m_map_point_count = 0;

    VulkanBuffer map_uniform_buffer;
    VulkanBuffer map_hash_table_buffer;
    VulkanBuffer map_point_buffer;
    VulkanBuffer map_normal_buffer;
    VulkanBuffer map_point_count_buffer;
};
