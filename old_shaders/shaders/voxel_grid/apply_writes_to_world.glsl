#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) coherent buffer ChunkHashTable { HashTableCounters chunk_hash_table_counters; ChunkHashTableSlot chunk_hash_table_slots[]; };
layout(std430, binding=1) readonly buffer VoxelWriteList { uint write_count; VoxelWrite writes[]; };
layout(std430, binding=2) buffer ChunkVoxels { VoxelData voxels[]; };
layout(std430, binding=3) buffer FreeList { uint free_count; uint free_list[]; };
layout(std430, binding=4) buffer ChunkMetaBuf { ChunkMeta meta[]; };
layout(std430, binding=5) buffer EnqueuedBuf { uint enqueued[]; };
layout(std430, binding=6) buffer DirtyListBuf { uint dirty_count; uint dirty_list[]; };

uniform uint  u_chunk_hash_table_size;
uniform ivec3 u_chunk_dim;
uniform uint  u_voxels_per_chunk;

uniform uint u_pack_bits;
uniform int  u_pack_offset;

// ----- include -----
#include "../utils.glsl"
#include "chunk_pool/voxel_index.glsl"
#include "chunk_pool/mark_dirty.glsl"
#include "chunk_hash_table/get_or_create.glsl"
// -------------------

// ---------- helpers ----------
void main() {
    uint wi = gl_GlobalInvocationID.x;
    uint writeCount = write_count;
    if (wi >= writeCount) return;

    VoxelWrite w = writes[wi];

    uint type = read_voxel_type(w.voxel_data.type_flags);
    if (type == 0u) return;

    ivec3 wc = w.world_voxel.xyz;

    ivec3 chunkCoord = ivec3(
        floor_div(wc.x, u_chunk_dim.x),
        floor_div(wc.y, u_chunk_dim.y),
        floor_div(wc.z, u_chunk_dim.z)
    );

    ivec3 local = ivec3(
        floor_mod(wc.x, u_chunk_dim.x),
        floor_mod(wc.y, u_chunk_dim.y),
        floor_mod(wc.z, u_chunk_dim.z)
    );

    uvec2 key = pack_key_uvec2(chunkCoord, u_pack_offset, u_pack_bits);

    uint slot_id;
    bool created;

    if (!chunk_hash_table_get_or_create_slot(key, slot_id, created)) return;
    uint chunk_id = chunk_hash_table_slots[slot_id].value;

    uint vi = voxel_index(local);

    // просто пишем VoxelData как есть (8 байт)
    voxels[chunk_id * u_voxels_per_chunk + vi] = w.voxel_data;

    mark_dirty_around(chunk_id, chunkCoord);
}
