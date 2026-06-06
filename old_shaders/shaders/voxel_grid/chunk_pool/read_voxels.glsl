#pragma once
#include "../../common/buffer_structures.glsl"
#include "../../utils.glsl"
#include "voxel_index.glsl"
#include "../chunk_hash_table/lookup_remove.glsl"


VoxelData voxel_data_in_chunk(uint chunkId, ivec3 p) {
    uint idx = chunkId * u_voxels_per_chunk + voxel_index(p);
    return voxels[idx];
}

VoxelData voxel_data_world(ivec3 chunkCoord, ivec3 local, out bool success) {
    uvec2 key = pack_key_uvec2(chunkCoord, u_pack_offset, u_pack_bits);
    uint slot_id = chunk_hash_table_lookup_hash_table_slot_id(key);
    if (slot_id == INVALID_ID) {
        success = false;
        VoxelData r;
        return r;
    }

    success = true;
    uint chunk_id = chunk_hash_table_slots[slot_id].value;
    return voxel_data_in_chunk(chunk_id, local);
}

VoxelData neighbor_voxel_data(uint chunkId, ivec3 chunkCoord, ivec3 p, ivec3 d, out bool success) {
    success = true;
    ivec3 q = p + d;
    if (q.x >= 0 && q.x < u_chunk_dim.x &&
        q.y >= 0 && q.y < u_chunk_dim.y &&
        q.z >= 0 && q.z < u_chunk_dim.z) {
        return voxel_data_in_chunk(chunkId, q);
    }

    ivec3 nChunk = chunkCoord;
    ivec3 nLocal = q;

    if (nLocal.x < 0) { nChunk.x -= 1; nLocal.x += u_chunk_dim.x; }
    if (nLocal.x >= u_chunk_dim.x) { nChunk.x += 1; nLocal.x -= u_chunk_dim.x; }

    if (nLocal.y < 0) { nChunk.y -= 1; nLocal.y += u_chunk_dim.y; }
    if (nLocal.y >= u_chunk_dim.y) { nChunk.y += 1; nLocal.y -= u_chunk_dim.y; }

    if (nLocal.z < 0) { nChunk.z -= 1; nLocal.z += u_chunk_dim.z; }
    if (nLocal.z >= u_chunk_dim.z) { nChunk.z += 1; nLocal.z -= u_chunk_dim.z; }

    return voxel_data_world(nChunk, nLocal, success);
}

// 0/1 видимость
uint voxel_vis(uint chunkId, ivec3 chunkCoord, ivec3 p, ivec3 d) {
    bool success;
    VoxelData voxel_data = neighbor_voxel_data(chunkId, chunkCoord, p, d, success);
    if (success)
        return (read_voxel_flags(voxel_data.type_flags) & VOXEL_VISABILITY_FLAG_BIT) > 0u ? 1u : 0u;
    else
        return 0u;
}
