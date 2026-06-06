#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) buffer VBHeads { uint vb_heads[]; };
layout(std430, binding=1) buffer VBState { uint vb_state[]; };
layout(std430, binding=2) buffer VBFreeNodesList  { uint vb_free_nodes_counter; uint vb_free_nodes_list[];  };
layout(std430, binding=3) buffer IBHeads { uint ib_heads[]; };
layout(std430, binding=4) buffer IBState { uint ib_state[]; };
layout(std430, binding=5) buffer IBFreeNodesList  { uint ib_free_nodes_counter; uint ib_free_nodes_list[];  };
layout(std430, binding=6) buffer ChunkMeshAllocBuf { ChunkMeshAlloc chunk_alloc[]; };

uniform uint u_vb_pages;
uniform uint u_ib_pages;
uniform uint u_vb_nodes;
uniform uint u_ib_nodes;
uniform uint u_vb_heads_count;
uniform uint u_ib_heads_count;
uniform uint u_max_chunks;

// ----- include -----
#include "../utils.glsl"
#include "../common/allocator.glsl"
// -------------------

void main() {
    uint i = gl_GlobalInvocationID.x;

    if (i == 0u) {
        vb_free_nodes_counter = u_vb_nodes;
        ib_free_nodes_counter = u_ib_nodes;
    }

    if (i < u_vb_pages) vb_state[i] = pack_state(0, ST_MERGED); // "ничего"
    if (i < u_ib_pages) ib_state[i] = pack_state(0, ST_MERGED);

    if (i < u_vb_nodes) vb_free_nodes_list[i] = i;
    if (i < u_ib_nodes) ib_free_nodes_list[i] = i;

    if (i < u_vb_heads_count) vb_heads[i] = pack_head(INVALID_HEAD_IDX, 0u);
    if (i < u_ib_heads_count) ib_heads[i] = pack_head(INVALID_HEAD_IDX, 0u);

    if (i < u_max_chunks) {
        chunk_alloc[i].v_startPage = INVALID_ID; 
        chunk_alloc[i].v_order = 0u; 
        chunk_alloc[i].needV = 0u; 
        chunk_alloc[i].i_startPage = INVALID_ID; 
        chunk_alloc[i].i_order = 0u; 
        chunk_alloc[i].needI = 0u; 
        chunk_alloc[i].is_valid = true;
    }
}
