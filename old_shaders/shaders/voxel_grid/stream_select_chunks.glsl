#version 430
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) coherent buffer ChunkHashTable { HashTableCounters chunk_hash_table_counters; ChunkHashTableSlot chunk_hash_table_slots[]; };
layout(std430, binding=1) buffer FreeList { uint free_count; uint free_list[]; };
layout(std430, binding=2) buffer ChunkMetaBuf { ChunkMeta meta[]; };
layout(std430, binding=3) buffer EnqueuedBuf { uint enqueued[]; };
layout(std430, binding=4) buffer LoadList { uint load_list_counter; uint load_list[]; };

uniform uint  u_chunk_hash_table_size;   // pow2
uniform uint  u_max_load_entries;  // обычно = count_active_chunks
uniform ivec3 u_chunk_dim;
uniform vec3  u_voxel_size;

uniform vec3  u_cam_pos_local;    // камера в локальных координатах грида
uniform int   u_radius_chunks;    // R в чанках

uniform uint u_pack_bits;
uniform int  u_pack_offset;

// ----- include -----
#include "../utils.glsl"
#include "chunk_hash_table/common.glsl"
#include "chunk_hash_table/get_or_create.glsl"
#include "chunk_hash_table/lookup_remove.glsl"
// -------------------

void main() {
    uvec3 gid = gl_GlobalInvocationID.xyz;

    int R = u_radius_chunks;
    uint side = uint(2 * R + 1);

    if (gid.x >= side || gid.y >= side || gid.z >= side) return;

    ivec3 off = ivec3(gid) - ivec3(R);
    int d2 = off.x*off.x + off.y*off.y + off.z*off.z;
    if (d2 > R*R) return;
    // if (d2 > 0.5) return;

    vec3 chunkWorldSize = vec3(u_chunk_dim) * u_voxel_size;
    ivec3 camChunk = ivec3(floor(u_cam_pos_local / chunkWorldSize));

    ivec3 chunkCoord = camChunk + off;
    uvec2 key = pack_key_uvec2(chunkCoord, u_pack_offset, u_pack_bits);

    uint slot_id;
    bool created;

    if (!chunk_hash_table_get_or_create_slot(key, slot_id, created)) return;

    uint chunk_id = chunk_hash_table_slots[slot_id].value;

    if ((meta[chunk_id].dirty_flags & NEED_GENERATION_FLAG_BIT) > 0u) {
        uint i = atomicAdd(load_list_counter, 1u);
        if (i < u_max_load_entries) load_list[i] = chunk_id;
        meta[chunk_id].dirty_flags &= ~NEED_GENERATION_FLAG_BIT;
    }
}
