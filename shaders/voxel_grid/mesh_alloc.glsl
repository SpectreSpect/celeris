#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) buffer MeshBuffersStatusBuf { uint is_vb_full; uint is_ib_full; }; // y = dirtyCount
layout(std430, binding=1) readonly buffer DirtyListBuf { uint dirty_count; uint dirty_list[]; };
layout(std430, binding=2) readonly buffer DirtyQuadCountBuf { uint dirty_quad_count[]; };
layout(std430, binding=3) readonly buffer ChunkMetaBuf { ChunkMeta meta[]; };
layout(std430, binding=4) buffer ChunkMeshAllocLocalBuf { ChunkMeshAlloc chunk_alloc_local[]; }; 
layout(std430, binding=5) readonly buffer ChunkMeshAllocGlobalBuf { ChunkMeshAlloc chunk_alloc_global[]; }; 
layout(std430, binding=6) coherent buffer BBHeads { uint bb_heads[]; };
layout(std430, binding=7) coherent buffer BBState { uint bb_state[]; };
layout(std430, binding=8) coherent buffer BBNodes  { Node bb_nodes[];  };
layout(std430, binding=9) coherent buffer BBFreeNodesList  { uint bb_free_nodes_counter; uint bb_free_nodes_list[];  };
layout(std430, binding=10) coherent buffer BBReturnedNodesList  { uint bb_returned_nodes_counter; uint bb_returned_nodes_list[]; };

uniform uint bb_pages;
uniform uint bb_page_elements;  // например 256
uniform uint bb_max_order;   // log2(u_bb_pages)
uniform uint bb_quad_size;
uniform uint u_is_vb_phase;

// ----- include -----
#include "../utils.glsl"

#define PREFIX bb
#include "../common/allocator.glsl"
// -------------------

bool is_vb_phase = u_is_vb_phase == 1u;

void main() {
    uint dirtyIdx = gl_GlobalInvocationID.x;
    uint dirtyCount = dirty_count;
    if (dirtyIdx >= dirtyCount) return;
    
    uint chunkId = dirty_list[dirtyIdx];

    if (is_vb_full == 1u || is_ib_full == 1u) {
        if (is_vb_phase){
            chunk_alloc_local[dirtyIdx].v_startPage = INVALID_ID;
            chunk_alloc_local[dirtyIdx].v_order = 0u;
            chunk_alloc_local[dirtyIdx].needV = 0u;
        } else {
            chunk_alloc_local[dirtyIdx].i_startPage = INVALID_ID;
            chunk_alloc_local[dirtyIdx].i_order = 0u;
            chunk_alloc_local[dirtyIdx].needI = 0u;
        }

        chunk_alloc_local[dirtyIdx].is_valid = false;
        return;
    }

    // мог быть уже выселен
    if (meta[chunkId].used == 0u) {
        if (is_vb_phase){
            chunk_alloc_local[dirtyIdx].v_startPage = INVALID_ID;
            chunk_alloc_local[dirtyIdx].v_order = 0u;
            chunk_alloc_local[dirtyIdx].needV = 0u;
        } else {
            chunk_alloc_local[dirtyIdx].i_startPage = INVALID_ID;
            chunk_alloc_local[dirtyIdx].i_order = 0u;
            chunk_alloc_local[dirtyIdx].needI = 0u;
        }
        
        chunk_alloc_local[dirtyIdx].is_valid = true;
        return;
    }

    uint quads = dirty_quad_count[dirtyIdx];

    // пустой меш
    if (quads == 0u) {
        if (is_vb_phase){
            chunk_alloc_local[dirtyIdx].v_startPage = INVALID_ID;
            chunk_alloc_local[dirtyIdx].v_order = 0u;
            chunk_alloc_local[dirtyIdx].needV = 0u;
        } else {
            chunk_alloc_local[dirtyIdx].i_startPage = INVALID_ID;
            chunk_alloc_local[dirtyIdx].i_order = 0u;
            chunk_alloc_local[dirtyIdx].needI = 0u;
        }
        
        chunk_alloc_local[dirtyIdx].is_valid = true;
        return;
    }

    uint needB = quads * bb_quad_size;
    uint bPages = div_up_u32(needB, bb_page_elements);
    uint bOrder = ceil_log2_u32(bPages);

    uint bStart = bb_alloc_pages(bOrder);
    if (bStart == INVALID_ID) {
        if (is_vb_phase){
            atomicExchange(is_vb_full, 1u);
            chunk_alloc_local[dirtyIdx].v_startPage = INVALID_ID;
            chunk_alloc_local[dirtyIdx].v_order = 0u;
            chunk_alloc_local[dirtyIdx].needV = 0u;
        } else {
            atomicExchange(is_ib_full, 1u);
            chunk_alloc_local[dirtyIdx].i_startPage = INVALID_ID;
            chunk_alloc_local[dirtyIdx].i_order = 0u;
            chunk_alloc_local[dirtyIdx].needI = 0u;
        }
        
        chunk_alloc_local[dirtyIdx].is_valid = false;
        return;
    }

    if (is_vb_phase) {
        chunk_alloc_local[dirtyIdx].v_startPage = bStart;
        chunk_alloc_local[dirtyIdx].v_order = bOrder;
        chunk_alloc_local[dirtyIdx].needV = needB;
    } else {
        chunk_alloc_local[dirtyIdx].i_startPage = bStart;
        chunk_alloc_local[dirtyIdx].i_order = bOrder;
        chunk_alloc_local[dirtyIdx].needI = needB;
    }

    chunk_alloc_local[dirtyIdx].is_valid = true;
}
