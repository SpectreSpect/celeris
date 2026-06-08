#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) buffer CounterHashTable { HashTableCounters counter_hash_table_counters; CounterHashTableSlot counter_hash_table_slots[]; };
layout(std430, binding=1) buffer ActiveChunkKeysList { uint active_chunk_keys_counter; uvec2 active_chunk_keys_list[]; };
layout(std430, binding=2) buffer TriangleIndicesList { uint triangle_counter; uint triangle_indices_list[]; };

uniform uint u_counter_hash_table_size;

// ----- include -----
#include "counter_hash_table/lookup_remove.glsl"
// -------------------

void main() {
    uint active_key_list_id = gl_GlobalInvocationID.x;
    if (active_key_list_id >= active_chunk_keys_counter) return;

    uvec2 key = active_chunk_keys_list[active_key_list_id];
    uint slot_id = counter_hash_table_lookup_hash_table_slot_id(key, true);
    if (slot_id == INVALID_ID) return;

    uint base = atomicAdd(triangle_counter, counter_hash_table_slots[slot_id].value.count_triangles);
    counter_hash_table_slots[slot_id].value.triangle_indices_base = base;
    counter_hash_table_slots[slot_id].value.triangle_emmit_counter = base;
}