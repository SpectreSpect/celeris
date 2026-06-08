#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) coherent buffer ChunkHashTable { HashTableCounters chunk_hash_table_counters; ChunkHashTableSlot chunk_hash_table_slots[]; };
layout(std430, binding=1) buffer FreeList { uint free_count; uint free_list[]; };
layout(std430, binding=2) buffer ChunkMetaBuf { ChunkMeta meta[]; };
layout(std430, binding=3) buffer EnqueuedBuf { uint enqueued[]; };
layout(std430, binding=4) coherent buffer BucketHeads { BucketHead bucket_heads[]; };
layout(std430, binding=5) coherent buffer BucketNext  { uint bucket_next[]; };
layout(std430, binding=6) buffer ChunkMeshAllocBuf { ChunkMeshAlloc chunk_alloc[]; };
layout(std430, binding=7) buffer EvictedChunksList { uint evicted_chunks_counter; uint evicted_chunks_list[]; };

uniform uint u_chunk_hash_table_size;
uniform uint u_bucket_count;

// ----- include -----
#include "../utils.glsl"
#include "chunk_hash_table/common.glsl"
#include "chunk_hash_table/lookup_remove.glsl"
// -------------------

// ABA проблемы не будет, так как везде используется либо только pop, либо только push (поэтому теги на heads пока не нужны)
uint pop_bucket(uint b) {
    for (;;) {
        uint h = atomicAdd(bucket_heads[b].id, 0u);
        if (h == INVALID_ID) return INVALID_ID;

        memoryBarrierBuffer();
        uint nxt = bucket_next[h];

        uint prev = atomicCompSwap(bucket_heads[b].id, h, nxt);
        if (prev == h) return h;
    }
}

void main() {
    uint enviction_id = gl_GlobalInvocationID.x;
    if (enviction_id >= evicted_chunks_counter) return;

    uint evict_pointer = 0u;
    int bucket_id = int(u_bucket_count) - 1;
    for (; bucket_id >= 0; --bucket_id) {
        evict_pointer += bucket_heads[bucket_id].count;
        if (evict_pointer > enviction_id) break;
    }

    uint victim = pop_bucket(uint(bucket_id));

    if (victim == INVALID_ID)  {
        // не нашли
        evicted_chunks_list[enviction_id] = INVALID_ID;
        return;
    }

    // мог уже стать свободным/неактивным
    if (meta[victim].used == 0u) {
        evicted_chunks_list[enviction_id] = INVALID_ID;
        return;
    };

    uvec2 key = uvec2(meta[victim].key_lo, meta[victim].key_hi);

    // выкидываем из таблицы
    chunk_hash_table_remove_slot_from_hash_table(key);

    // освобождаем метаданные
    meta[victim].used = 0u;
    meta[victim].dirty_flags = NEED_GENERATION_FLAG_BIT;
    enqueued[victim] = 0u;

    // пушим обратно в free_list
    free_list[free_count + enviction_id] = victim;
    
    evicted_chunks_list[enviction_id] = victim;
}
