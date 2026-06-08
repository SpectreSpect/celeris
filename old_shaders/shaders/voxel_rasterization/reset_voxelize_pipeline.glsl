#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) buffer CounterHashTable { HashTableCounters counter_hash_table_counters; CounterHashTableSlot counter_hash_table_slots[]; };
layout(std430, binding=1) writeonly buffer ActiveChunkKeysList { uint active_chunk_keys_counter; uvec2 active_chunk_keys_list[]; };
layout(std430, binding=2) writeonly buffer TriangleIndicesList { uint triangle_counter; uint triangle_indices_list[]; };
layout(std430, binding=3) writeonly buffer VoxelsWriteData { uint count_voxel_writes; uint pad_[3u]; VoxelWrite voxel_writes[]; };

uniform uint u_counter_hash_table_size;
uniform uint u_reset_voxel_write_list;

// ----- include -----
#include "counter_hash_table/common.glsl"
// -------------------

void main() {
    uint slot_id = gl_GlobalInvocationID.x;

    if (slot_id == 0u) {
        reset_all_counters_as_empty();
        active_chunk_keys_counter = 0u;
        triangle_counter = 0u;
        if (u_reset_voxel_write_list == 1u) count_voxel_writes = 0u;
    }

    if (slot_id < u_counter_hash_table_size) {
        counter_hash_table_slots[slot_id].state = SLOT_EMPTY; // В теории достаточно только пометить все слоты пустыми
    }
}