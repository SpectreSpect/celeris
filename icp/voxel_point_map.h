#pragma once
#include "../vulkan_engine.h"
#include "../vulkan/video_buffer.h"
#include "../point_cloud/point_instance.h"
#include "../point_cloud/point_cloud_frame.h"


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

    VoxelPointMap() = default;
    void create(VulkanEngine& engine, uint32_t num_hash_table_slots, uint32_t max_map_point_count);
    void get_point_cloud_frame(PointCloudFrame* frame);

    uint32_t num_hash_table_slots = 0;
    uint32_t max_map_point_count = 0;
    uint32_t map_point_count = 0;

    VulkanEngine* engine = nullptr;
    VideoBuffer map_uniform_buffer;
    VideoBuffer map_hash_table_buffer;
    VideoBuffer map_point_buffer;
    VideoBuffer map_normal_buffer;
    VideoBuffer map_point_count_buffer;
};