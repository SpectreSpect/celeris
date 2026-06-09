#version 430
layout(local_size_x = 1) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) coherent buffer ChunkHashTable { HashTableCounters chunk_hash_table_counters; ChunkHashTableSlot chunk_hash_table_slots[]; };
layout(std430, binding=1) buffer ClearDispathArgs { uvec3 clear_dispatch_args; };
layout(std430, binding=2) buffer FillDispathArgs { uvec3 fill_dispatch_args; };

uniform uint u_chunk_hash_table_size;
uniform uint u_max_chunks;
uniform uint u_tombs_to_rebuild;

// ----- include -----
#include "../utils.glsl"
#include "chunk_hash_table/common.glsl"
// -------------------

void main() {
    if (gl_GlobalInvocationID.x != 0u) return;

    uint count_tomb = chunk_hash_table_reduce_read_tomb_counter();

    uint slot_groups = count_tomb >= u_tombs_to_rebuild ? div_up_u32(u_chunk_hash_table_size, 256u) : 0u;
    uint chunk_groups = count_tomb >= u_tombs_to_rebuild ? div_up_u32(u_max_chunks, 256u) : 0u;

    clear_dispatch_args = uvec3(slot_groups, 1u, 1u);
    fill_dispatch_args = uvec3(chunk_groups, 1u, 1u);
    
    if (count_tomb >= u_tombs_to_rebuild) 
        reset_all_counters_as_empty();
}
