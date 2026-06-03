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
