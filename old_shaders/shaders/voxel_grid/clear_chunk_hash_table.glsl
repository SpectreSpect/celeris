#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

// --- hash table ---
layout(std430, binding=0) coherent buffer ChunkHashTable { HashTableCounters chunk_hash_table_counters; ChunkHashTableSlot chunk_hash_table_slots[]; };

uniform uint u_chunk_hash_table_size;

// ----- include -----
#include "chunk_hash_table/common.glsl"
// -------------------

void main() {
    uint slot_id = gl_GlobalInvocationID.x;
    if (slot_id >= u_chunk_hash_table_size) return;

    chunk_hash_table_slots[slot_id].key = uvec2(0u, 0u);
    chunk_hash_table_slots[slot_id].state = SLOT_EMPTY;
}
