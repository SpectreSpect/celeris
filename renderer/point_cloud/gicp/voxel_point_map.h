#pragma once

#include "../../../vulkan_self/pass/instance/pass_instance.h"
#include "../../../vulkan_self/vulkan_buffer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

class InstanceBufferView;

class VoxelPointMap {
public:
    struct VoxelPointMapUniform {
        uint32_t source_point_count;
        uint32_t max_map_point_count;
    };

    // struct VoxelHashSlot {
    //     uint32_t state;
    //     uint32_t pad0[3];
    //     glm::ivec4 key;
    //     uint32_t value;
    //     uint32_t pad1[3];
    // };

    struct alignas(16) VoxelHashSlotGpu {
        uint32_t state;
        uint32_t pad0[3];
        glm::ivec4 key;
        uint32_t value;
        uint32_t pad1[3];
    };
    static_assert(sizeof(VoxelHashSlotGpu) == 48);

    VoxelPointMap(VulkanEngine& engine, uint32_t num_hash_table_slots, uint32_t max_map_point_count);

    InstanceBufferView get_map_instance_view();

    // VoxelPointMap() = default;
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