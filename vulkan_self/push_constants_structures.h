#pragma once

#include <cstdint>

struct FillBufferPushConstants {
    uint32_t prefab_data_bytes;
    uint32_t clearable_data_bytes;

    uint32_t clearable_data_offset_bytes;

    uint32_t count_invocations_x;
    uint32_t count_invocations_y;
    uint32_t count_invocations_z;

    uint32_t invocation_stride_bytes;
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

struct MeshGridPushConstants {
    uint32_t u_chunk_hash_table_size;
    uint32_t u_chunk_dim_x;
    uint32_t u_chunk_dim_y;
    uint32_t u_chunk_dim_z;
    uint32_t u_voxels_per_chunk;
    uint32_t u_pack_bits;
    int32_t u_pack_offset;
};

struct MeshAllocPushConstants {
    uint32_t bb_pages;
    uint32_t bb_page_elements;
    uint32_t bb_max_order;
    uint32_t bb_quad_size;
    uint32_t u_is_vb_phase;
};

struct MeshEmitPushConstants {
    uint32_t u_chunk_hash_table_size;
    uint32_t u_chunk_dim_x;
    uint32_t u_chunk_dim_y;
    uint32_t u_chunk_dim_z;
    uint32_t u_voxels_per_chunk;
    float u_voxel_size_x;
    float u_voxel_size_y;
    float u_voxel_size_z;
    uint32_t u_pack_bits;
    int32_t u_pack_offset;
    uint32_t u_vb_page_verts;
    uint32_t u_ib_page_inds;
};

struct VerifyMeshAllocationPushConstants {
    uint32_t vb_max_order;
    uint32_t ib_max_order;
};

struct MarkAllUsedChunksDirtyPushConstants {
    uint32_t u_max_chunks;
};

struct StreamSelectChunksPushConstants {
    uint32_t u_chunk_hash_table_size;
    uint32_t u_max_load_entries;
    uint32_t u_chunk_dim_x;
    uint32_t u_chunk_dim_y;
    uint32_t u_chunk_dim_z;
    float u_voxel_size_x;
    float u_voxel_size_y;
    float u_voxel_size_z;
    float u_cam_pos_local_x;
    float u_cam_pos_local_y;
    float u_cam_pos_local_z;
    int32_t u_radius_chunks;
    uint32_t u_pack_bits;
    int32_t u_pack_offset;
};

struct StreamGenerateTerrainPushConstants {
    uint32_t u_chunk_dim_x;
    uint32_t u_chunk_dim_y;
    uint32_t u_chunk_dim_z;
    uint32_t u_voxels_per_chunk;
    uint32_t u_pack_bits;
    int32_t u_pack_offset;
    uint32_t u_seed;
    uint32_t u_chunk_hash_table_size;
};

struct EvictBucketsBuildPushConstants {
    uint32_t u_max_chunks;
    uint32_t u_bucket_count;
    float u_cam_pos_x;
    float u_cam_pos_y;
    float u_cam_pos_z;
    uint32_t u_chunk_dim_x;
    uint32_t u_chunk_dim_y;
    uint32_t u_chunk_dim_z;
    float u_voxel_size_x;
    float u_voxel_size_y;
    float u_voxel_size_z;
    uint32_t u_pack_bits;
    int32_t u_pack_offset;
    float f_eviction_bucket_shell_thickness;
};

struct EvictLowPriorityPushConstants {
    uint32_t u_chunk_hash_table_size;
    uint32_t u_bucket_count;
};

struct ResetEvictedListAndBucketsPushConstants {
    uint32_t u_bucket_count;
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
