#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) coherent buffer ChunkHashTable { HashTableCounters chunk_hash_table_counters; ChunkHashTableSlot chunk_hash_table_slots[]; };
layout(std430, binding=1) buffer ChunkMetaBuf { ChunkMeta meta[]; };
layout(std430, binding=2) buffer EnqueuedBuf { uint enqueued[]; };

uniform uint u_max_chunks;
uniform uint u_chunk_hash_table_size;
uniform uint u_pack_bits;
uniform int u_pack_offset;

// ----- include -----
#include "../utils.glsl"
#include "chunk_hash_table/common.glsl"
#include "chunk_hash_table/lookup_remove.glsl"
// -------------------

void main() {
    uint chunk_id = gl_GlobalInvocationID.x;
    if (chunk_id >= u_max_chunks) return;

    ChunkMeta chunk_meta = meta[chunk_id];
    if (chunk_meta.used == 0u) return;

    uvec2 key = uvec2(chunk_meta.key_lo, chunk_meta.key_hi);

    chunk_hash_table_set_slot_value(key, chunk_id);
}