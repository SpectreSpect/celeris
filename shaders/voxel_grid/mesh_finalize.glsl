#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) readonly buffer DirtyListBuf { uint dirty_count; uint dirty_list[]; };
layout(std430, binding=1) buffer EnqueuedBuf { uint enqueued[]; };
layout(std430, binding=2) buffer ChunkMetaBuf { ChunkMeta meta[]; };
layout(std430, binding=3) buffer ChunkMeshAllocBuf { ChunkMeshAlloc chunk_alloc[]; }; 
layout(std430, binding=4) buffer FailedDirtyListBuf { uint failed_dirty_count; uint failed_dirty_list[]; }; 

// ----- include -----
#include "../utils.glsl"
// -------------------

void main() {
    uint dirtyIdx = gl_GlobalInvocationID.x;
    uint dirtyCount = dirty_count;
    if (dirtyIdx >= dirtyCount) return;

    uint chunkId = dirty_list[dirtyIdx];
    
    if (!chunk_alloc[chunkId].is_valid) {
        if (enqueued[chunkId] != 2u) {
            uint idx = atomicAdd(failed_dirty_count, 1u);
            failed_dirty_list[idx] = chunkId;
            enqueued[chunkId] = 2u;
        }
    } else {
        // разрешить повторно enqueue
        enqueued[chunkId] = 0u;
    }

    // снять dirty флаг(и)
    meta[chunkId].dirty_flags &= ~DIRTY_FLAG_BIT;
}
