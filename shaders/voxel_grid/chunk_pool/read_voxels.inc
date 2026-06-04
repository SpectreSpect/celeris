#ifndef VOXEL_GRID_CHUNK_POOL_READ_VOXELS_GLSL
#define VOXEL_GRID_CHUNK_POOL_READ_VOXELS_GLSL

#include "voxel_index.inc"
#include "../chunk_hash_table/lookup_remove.inc"

VoxelData voxel_data_in_chunk(uint chunk_id, uvec3 p) {
    uint idx = chunk_id * u_voxels_per_chunk + voxel_index(p);
    return voxels[idx];
}

VoxelData voxel_data_world(ivec3 chunk_coord, uvec3 local, out bool success) {
    uvec2 key = pack_key_uvec2(chunk_coord, u_pack_offset, u_pack_bits);
    uint slot_id = chunk_hash_table_lookup_hash_table_slot_id(key);
    if (slot_id == INVALID_ID) {
        success = false;
        VoxelData result;
        result.type_flags = 0u;
        result.color = 0u;
        return result;
    }

    success = true;
    uint chunk_id = chunk_hash_table_slots[slot_id].value;
    return voxel_data_in_chunk(chunk_id, local);
}

VoxelData neighbor_voxel_data(uint chunk_id, ivec3 chunk_coord, uvec3 p, ivec3 d, out bool success) {
    ivec3 q = ivec3(p) + d;
    if (q.x >= 0 && q.x < int(u_chunk_dim_x) &&
        q.y >= 0 && q.y < int(u_chunk_dim_y) &&
        q.z >= 0 && q.z < int(u_chunk_dim_z)) {
        success = true;
        return voxel_data_in_chunk(chunk_id, uvec3(q));
    }

    ivec3 n_chunk = chunk_coord;
    ivec3 n_local = q;

    if (n_local.x < 0) { n_chunk.x -= 1; n_local.x += int(u_chunk_dim_x); }
    if (n_local.x >= int(u_chunk_dim_x)) { n_chunk.x += 1; n_local.x -= int(u_chunk_dim_x); }

    if (n_local.y < 0) { n_chunk.y -= 1; n_local.y += int(u_chunk_dim_y); }
    if (n_local.y >= int(u_chunk_dim_y)) { n_chunk.y += 1; n_local.y -= int(u_chunk_dim_y); }

    if (n_local.z < 0) { n_chunk.z -= 1; n_local.z += int(u_chunk_dim_z); }
    if (n_local.z >= int(u_chunk_dim_z)) { n_chunk.z += 1; n_local.z -= int(u_chunk_dim_z); }

    return voxel_data_world(n_chunk, uvec3(n_local), success);
}

uint voxel_vis(uint chunk_id, ivec3 chunk_coord, uvec3 p, ivec3 d) {
    bool success;
    VoxelData voxel_data = neighbor_voxel_data(chunk_id, chunk_coord, p, d, success);
    if (!success) {
        return 0u;
    }

    return (read_voxel_flags(voxel_data.type_flags) & VOXEL_VISABILITY_FLAG_BIT) != 0u ? 1u : 0u;
}

#endif // VOXEL_GRID_CHUNK_POOL_READ_VOXELS_GLSL
