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
