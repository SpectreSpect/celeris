#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) buffer ChunkHashTable { HashTableCounters chunk_hash_table_counters; ChunkHashTableSlot chunk_hash_table_slots[]; };
layout(std430, binding=1) buffer FreeList { uint free_count; uint free_list[]; };
layout(std430, binding=2) buffer MeshBuffersStatusBuf { uint is_vb_full; uint is_ib_full; };
layout(std430, binding=3) buffer ChunkMetaBuf { ChunkMeta meta[]; };
layout(std430, binding=4) buffer EnqueuedBuf { uint enqueued[]; };
layout(std430, binding=5) buffer DirtyListBuf { uint dirty_count; uint dirty_list[]; };
layout(std430, binding=6) buffer VoxelWriteList { uint write_count; VoxelWrite writes[]; };
layout(std430, binding=7) buffer IndirectCmdBuf { uint cmd_count; DrawElementsIndirectCommand cmds[]; };
layout(std430, binding=8) buffer FailedDirtyListBuf { uint failed_dirty_count; uint failed_dirty_list[]; }; 

uniform uint u_chunk_hash_table_size;
uniform uint u_max_chunks;

// ----- include -----
#include "chunk_hash_table/common.glsl"
// -------------------

void main() {
    uint i = gl_GlobalInvocationID.x;

    if (i < u_chunk_hash_table_size) {
        chunk_hash_table_slots[i].value = INVALID_ID;
        chunk_hash_table_slots[i].state = SLOT_EMPTY;
        chunk_hash_table_slots[i].key = uvec2(0u, 0u);
    }

    if (i < u_max_chunks) {
        free_list[i] = i;
        enqueued[i] = 0u;

        meta[i].used = 0u;
        meta[i].key_lo = 0u;
        meta[i].key_hi = 0u;
        meta[i].dirty_flags = NEED_GENERATION_FLAG_BIT;
    }

    if (i == 0u) {
        write_count = 0u; 
        dirty_count = 0u; 
        cmd_count = 0u; 
        free_count = u_max_chunks; 
        failed_dirty_count = 0u;
        
        is_vb_full = 0u;
        is_ib_full = 0u;

        reset_all_counters_as_empty();
    }
}
