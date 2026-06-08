#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) coherent buffer ChunkHashTable { HashTableCounters chunk_hash_table_counters; ChunkHashTableSlot chunk_hash_table_slots[]; };
layout(std430, binding=1) readonly buffer ChunkVoxels { VoxelData voxels[]; };
layout(std430, binding=2) readonly buffer DirtyListBuf { uint dirty_count; uint dirty_list[]; };
layout(std430, binding=3) buffer DirtyQuadCountBuf { uint dirty_quad_count[]; };
layout(std430, binding=4) readonly buffer ChunkMetaBuf { ChunkMeta meta[]; };

uniform uint  u_chunk_hash_table_size;
uniform ivec3 u_chunk_dim;
uniform uint  u_voxels_per_chunk;

uniform uint u_pack_bits;
uniform int  u_pack_offset;

// ----- include -----
#include "../utils.glsl"
#include "chunk_pool/read_voxels.glsl"
// -------------------

void main() {
    uint voxelId  = gl_GlobalInvocationID.x; // 0..4095
    uint dirtyIdx = gl_GlobalInvocationID.y;

    uint dirtyCount = dirty_count;
    if (dirtyIdx >= dirtyCount) return;
    if (voxelId >= u_voxels_per_chunk) return;

    uint chunkId = dirty_list[dirtyIdx];

    uvec2 key2 = uvec2(meta[chunkId].key_lo, meta[chunkId].key_hi);
    ivec3 chunkCoord = unpack_key_to_coord(key2, u_pack_offset, u_pack_bits);

    int lx = int(voxelId % uint(u_chunk_dim.x));
    int ly = int((voxelId / uint(u_chunk_dim.x)) % uint(u_chunk_dim.y));
    int lz = int(voxelId / uint(u_chunk_dim.x * u_chunk_dim.y));
    ivec3 p = ivec3(lx, ly, lz);

    VoxelData voxel_data = voxel_data_in_chunk(chunkId, p);
    if ((read_voxel_flags(voxel_data.type_flags) & VOXEL_VISABILITY_FLAG_BIT) == 0u) return;

    if (voxel_vis(chunkId, chunkCoord, p, ivec3( 1, 0, 0)) == 0u) atomicAdd(dirty_quad_count[dirtyIdx], 1u);
    if (voxel_vis(chunkId, chunkCoord, p, ivec3(-1, 0, 0)) == 0u) atomicAdd(dirty_quad_count[dirtyIdx], 1u);
    if (voxel_vis(chunkId, chunkCoord, p, ivec3( 0, 1, 0)) == 0u) atomicAdd(dirty_quad_count[dirtyIdx], 1u);
    if (voxel_vis(chunkId, chunkCoord, p, ivec3( 0,-1, 0)) == 0u) atomicAdd(dirty_quad_count[dirtyIdx], 1u);
    if (voxel_vis(chunkId, chunkCoord, p, ivec3( 0, 0, 1)) == 0u) atomicAdd(dirty_quad_count[dirtyIdx], 1u);
    if (voxel_vis(chunkId, chunkCoord, p, ivec3( 0, 0,-1)) == 0u) atomicAdd(dirty_quad_count[dirtyIdx], 1u);
}
