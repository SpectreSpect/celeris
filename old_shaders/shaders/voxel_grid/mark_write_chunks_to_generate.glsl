#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) readonly buffer VoxelsWriteData { uint count_voxel_writes; uint pad_[3u]; VoxelWrite voxel_writes[]; };
layout(std430, binding=1) buffer LoadList { uint load_list_counter; uint load_list[]; };
layout(std430, binding=2) coherent buffer ChunkHashTable { HashTableCounters chunk_hash_table_counters; ChunkHashTableSlot chunk_hash_table_slots[]; };
layout(std430, binding=3) buffer FreeList { uint free_count; uint free_list[]; };
layout(std430, binding=4) buffer ChunkMetaBuf { ChunkMeta meta[]; };

uniform uvec3 u_chunk_dim;

uniform uint u_chunk_hash_table_size;
uniform uint u_pack_offset;
uniform uint u_pack_bits;

// ----- include -----
#include "../utils.glsl"
#include "chunk_hash_table/common.glsl"
#include "chunk_hash_table/get_or_create.glsl"
#include "chunk_hash_table/lookup_remove.glsl"
// -------------------

void main() {
    uint voxel_write_id = gl_GlobalInvocationID.x;
    if (voxel_write_id >= count_voxel_writes) return;

    ivec3 chunk_pos = floor_div(voxel_writes[voxel_write_id].world_voxel.xyz, ivec3(u_chunk_dim));
    uvec2 key = pack_key_uvec2(chunk_pos, u_pack_offset, u_pack_bits);

    uint slot_id;
    bool created;
    if (!chunk_hash_table_get_or_create_slot(key, slot_id, created)) return;

    uint chunk_id = chunk_hash_table_slots[slot_id].value;

    uint old_dirty_flags = atomicAdd(meta[chunk_id].dirty_flags, 0u);        
    if ((old_dirty_flags & NEED_GENERATION_FLAG_BIT) == 0u) return;

    uint new_dirty_flags = old_dirty_flags & ~NEED_GENERATION_FLAG_BIT;
    if (atomicCompSwap(meta[chunk_id].dirty_flags, old_dirty_flags, new_dirty_flags) == old_dirty_flags) {
        uint load_chunk_id = atomicAdd(load_list_counter, 1u);
        load_list[load_chunk_id] = chunk_id;
    }
}
