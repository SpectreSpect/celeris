#pragma once

#include <cstdint>

inline constexpr uint32_t PREFAB_MAX_UINTS = 25u;

struct FillBufferPushConstants {
    uint32_t prefab_data_bytes;
    uint32_t clearable_data_bytes;

    uint32_t clearable_data_offset_bytes;

    uint32_t count_invocations_x;
    uint32_t count_invocations_y;
    uint32_t count_invocations_z;

    uint32_t invocation_stride_bytes;

    uint32_t prefab_data[PREFAB_MAX_UINTS];
};

struct WorldInitPushConstants {
    uint32_t u_chunk_hash_table_size;
    uint32_t u_max_chunks;
};

struct ApplyVoxelWritesPushConstants {
    uint32_t u_chunk_hash_table_size;
    uint32_t u_chunk_dim_x;
    uint32_t u_chunk_dim_y;
    uint32_t u_chunk_dim_z;
    uint32_t u_voxels_per_chunk;
    uint32_t u_pack_bits;
    int32_t u_pack_offset;
};

struct DispatchAdapterPushConstants {
    uint32_t u_offset_bytes_0;
    uint32_t u_offset_bytes_1;
    uint32_t u_offset_bytes_2;

    uint32_t u_direct_value_0;
    uint32_t u_direct_value_1;
    uint32_t u_direct_value_2;

    uint32_t u_x_workgroup_size;
    uint32_t u_y_workgroup_size;
    uint32_t u_z_workgroup_size;
};

struct alignas(16) StreamSelectChunksPushConstants {
    uint32_t u_chunk_hash_table_size;
    uint32_t u_max_load_entries;
    uint32_t _pad0;
    uint32_t _pad1;

    glm::ivec4 u_chunk_dim;
    glm::vec4 u_voxel_size;

    glm::vec4 u_cam_pos_local;

    int32_t u_radius_chunks;
    uint32_t u_pack_bits;
    int32_t u_pack_offset;
    uint32_t _pad2;
};

static_assert(offsetof(StreamSelectChunksPushConstants, u_chunk_dim) == 16);
static_assert(offsetof(StreamSelectChunksPushConstants, u_voxel_size) == 32);
static_assert(offsetof(StreamSelectChunksPushConstants, u_cam_pos_local) == 48);
static_assert(offsetof(StreamSelectChunksPushConstants, u_radius_chunks) == 64);

struct MeshCountPushConstants {
    glm::ivec4 u_chunk_dim;
    uint32_t  u_chunk_hash_table_size;
    uint32_t  u_voxels_per_chunk;

    uint32_t u_pack_bits;
    int32_t  u_pack_offset;
};

struct MeshAllocPushConstants {
    uint32_t bb_pages;
    uint32_t bb_page_elements;  // например 256
    uint32_t bb_max_order;   // log2(u_bb_pages)
    uint32_t bb_quad_size;
    uint32_t u_is_vb_phase;
};

struct VerifyMeshAllocationPushConstants {
    uint32_t vb_max_order;
    uint32_t ib_max_order;
};

struct ReturnFreeAllocNodesPushConstants {
    glm::uvec4 u3_chunk_size;
};

struct MarkWriteChunksToGeneratePushConstants {
    glm::uvec4 u_chunk_dim;

    uint32_t u_chunk_hash_table_size;
    uint32_t u_pack_offset;
    uint32_t u_pack_bits;
};

struct MeshEmitPushConstants {
    glm::ivec4 u_chunk_dim;
    glm::vec4 u_voxel_size;

    uint32_t u_pack_bits;
    int32_t  u_pack_offset;
    uint32_t u_vb_page_verts;
    uint32_t u_ib_page_inds;

    uint32_t u_chunk_hash_table_size;
    uint32_t u_voxels_per_chunk;
};

struct StreamGenerateTerrainPushConstants {
    glm::ivec4 u_chunk_dim;
    uint32_t u_voxels_per_chunk;

    uint32_t u_pack_bits;
    int32_t  u_pack_offset;

    uint32_t u_seed;

    uint32_t u_chunk_hash_table_size;
};

struct WriteVoxelsToGridPushConstants {
    glm::ivec4 u_chunk_dim;
    uint32_t u_chunk_hash_table_size;

    uint32_t u_voxels_per_chunk;

    uint32_t u_pack_offset;
    uint32_t u_pack_bits;
};

struct EvictBucketsBuildPushConstants {
    glm::vec4 u_cam_pos;
    glm::ivec4 u_chunk_dim;
    glm::vec4 u_voxel_size;

    uint32_t u_max_chunks;
    uint32_t u_bucket_count;

    uint32_t u_pack_bits;
    int32_t u_pack_offset;

    float f_eviction_bucket_shell_thickness;
};

struct EvictLowPriorityDispatchAdapterPushConstants {
    uint32_t u_min_free;
};

struct EvictLowPriorityPushConstants {
    uint32_t u_chunk_hash_table_size;
    uint32_t u_bucket_count;
};

struct BuildIndirectCmdsPushConstants {
    glm::vec4 cam_pos;
    // 6 плоскостей фрустума в world space: ax+by+cz+d >= 0 (внутри)
    glm::vec4 u_frustum_planes[6];
};

struct FreeEvictedChunksMeshPushConstants {
    uint32_t vb_max_order;
    uint32_t ib_max_order;
};

struct ResetEvictedListAndBucketsPushConstants {
    uint32_t u_bucket_count;
};

struct HashTableConditionalDispatchAdapterPushConstants {
    uint32_t u_chunk_hash_table_size;
    uint32_t u_max_chunks;
    uint32_t u_tombs_to_rebuild;
};

struct ClearChunkHashTablePushConstants {
    uint32_t u_chunk_hash_table_size;
};

struct FillChunkHashTablePushConstants {
    uint32_t u_max_chunks;
    uint32_t u_chunk_hash_table_size;
    uint32_t u_pack_bits;
    int32_t u_pack_offset;
};

struct VoxelListFromPointCloudPushConstants {
    glm::mat4 source_point_cloud_model;
    glm::vec4 voxel_size;
    uint32_t point_count;
    uint32_t max_write_count;
};

static_assert(offsetof(VoxelListFromPointCloudPushConstants, source_point_cloud_model) == 0);
static_assert(offsetof(VoxelListFromPointCloudPushConstants, voxel_size) == 64);
static_assert(offsetof(VoxelListFromPointCloudPushConstants, point_count) == 80);
static_assert(offsetof(VoxelListFromPointCloudPushConstants, max_write_count) == 84);

struct AllocActiveChunkTrianglesPushConstants {
    uint32_t u_counter_hash_table_size;
};

struct FillTriangleIndicesPushConstants {
    glm::vec4 u_voxel_size;
    glm::uvec4 u_chunk_size;
    glm::mat4 u_transform;

    uint32_t u_count_mesh_triangles;
    uint32_t u_counter_hash_table_size;

    uint32_t u_vertex_stride_bytes;
    uint32_t u_vertex_position_offset_bytes;

    uint32_t u_pack_offset;
    uint32_t u_pack_bits;
};

struct MarkAndCountActiveChunksPushConstants {
    glm::vec4 u_voxel_size;
    glm::uvec4 u_chunk_size;
    glm::mat4 u_transform;

    uint32_t u_count_mesh_triangles;

    uint32_t u_counter_hash_table_size;

    uint32_t u_vertex_stride_bytes;
    uint32_t u_vertex_position_offset_bytes;

    uint32_t u_pack_offset;
    uint32_t u_pack_bits;
};

struct ResetVoxelizePipelinePushConstants {
    uint32_t u_counter_hash_table_size;
    uint32_t u_reset_voxel_write_list;
};


struct VoxelizeTrianglesPushConstants {
    glm::uvec4 u_chunk_dim;
    glm::vec4 u_voxel_size;

    glm::mat4 u_transform;

    uint32_t u_counter_hash_table_size;
    uint32_t u_count_voxels_in_chunk;

    uint32_t u_vertex_stride_bytes;
    uint32_t u_vertex_position_offset_bytes;

    uint32_t u_pack_offset;
    uint32_t u_pack_bits;

    uint32_t voxel_type_flags;
    uint32_t voxel_color;
    uint32_t voxel_set_flags;
};

struct GenerateMeshPushConstants {
    uint32_t count_triangles_in_lidar_ring;
    uint32_t count_points_in_lidar_ring;

    uint32_t point_stride_bytes;
    uint32_t point_position_offset_bytes;
    
    uint32_t vertex_stride_bytes;
    uint32_t vertex_position_offset_bytes;
    uint32_t vertex_normal_offset_bytes;
    uint32_t vertex_color_offset_bytes;
};

struct NormalsFromWebotsLidarPointCloudPushConstants {
    uint32_t point_count;
    uint32_t ring_count;
    uint32_t ring_width;
};

struct RemoveNearOriginLidarPointsPushConstants {
    uint32_t point_count;
    float min_distance;
};
