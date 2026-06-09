#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) coherent buffer ChunkHashTable { HashTableCounters chunk_hash_table_counters; ChunkHashTableSlot chunk_hash_table_slots[]; };
layout(std430, binding=1) buffer FreeList { uint free_count; uint free_list[]; };
layout(std430, binding=2) buffer ChunkMetaBuf { ChunkMeta meta[]; };
layout(std430, binding=3) buffer EnqueuedBuf { uint enqueued[]; };
layout(std430, binding=4) buffer DirtyListBuf { uint dirty_count; uint dirty_list[]; };

layout(std430, binding=5) readonly buffer VoxelsWriteData { uint count_voxel_writes; uint pad_[3u]; VoxelWrite voxel_writes[]; };
layout(std430, binding=6) buffer ChunkVoxels { VoxelData voxels[]; };

uniform uint u_chunk_hash_table_size;
uniform ivec3 u_chunk_dim;
uniform uint u_voxels_per_chunk;

uniform uint u_pack_offset;
uniform uint u_pack_bits;

// ----- include -----
#include "../utils.glsl"

#include "chunk_hash_table/common.glsl"
#include "chunk_hash_table/get_or_create.glsl"
#include "chunk_hash_table/lookup_remove.glsl"

#include "chunk_pool/mark_dirty.glsl"
#include "chunk_pool/voxel_index.glsl"
// -------------------

void main() {
    uint voxel_write_id = gl_GlobalInvocationID.x;
    if (voxel_write_id >= count_voxel_writes) return;

    VoxelWrite voxel_write = voxel_writes[voxel_write_id];
    ivec3 chunk_pos = floor_div(voxel_write.world_voxel.xyz, u_chunk_dim);
    ivec3 local_voxel_pos = voxel_write.world_voxel.xyz - chunk_pos * u_chunk_dim;
    
    uvec2 key = pack_key_uvec2(chunk_pos, u_pack_offset, u_pack_bits);

    uint slot_id;
    bool created;
    if (!chunk_hash_table_get_or_create_slot(key, slot_id, created)) return;

    uint chunk_id = chunk_hash_table_slots[slot_id].value;
    
    uint voxel_id = voxel_index(local_voxel_pos);
    uint global_voxel_id = chunk_id * u_voxels_per_chunk + voxel_id;

    if ((voxel_write.set_flags & OVERWRITE_BIT) > 0u) {
        voxels[global_voxel_id] = voxel_write.voxel_data;

        mark_dirty_around(chunk_id, chunk_pos);
    }
    else {
        if ((read_voxel_flags(voxels[global_voxel_id].type_flags) & VOXEL_EASY_OVERWRITE_FLAG_BIT) > 0u) {
            voxels[global_voxel_id] = voxel_write.voxel_data;
            mark_dirty_around(chunk_id, chunk_pos);
        }
    }
}
