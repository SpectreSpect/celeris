#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) buffer GlobalChunkMeshAllocBuf { ChunkMeshAlloc chunk_alloc_global[]; }; 
layout(std430, binding=1) coherent buffer VBHeads { uint vb_heads[]; };
layout(std430, binding=2) coherent buffer VBState { uint vb_state[]; };
layout(std430, binding=3) coherent buffer VBNodes  { Node vb_nodes[];  };
layout(std430, binding=4) coherent buffer VBFreeNodesList  { uint vb_free_nodes_counter; uint vb_free_nodes_list[];  };
layout(std430, binding=5) coherent buffer VBReturnedNodesList  { uint vb_returned_nodes_counter; uint vb_returned_nodes_list[]; };
layout(std430, binding=6) coherent buffer IBHeads { uint ib_heads[]; };
layout(std430, binding=7) coherent buffer IBState { uint ib_state[]; };
layout(std430, binding=8) coherent buffer IBNodes { Node ib_nodes[];  };
layout(std430, binding=9) coherent buffer IBFreeNodesList  { uint ib_free_nodes_counter; uint ib_free_nodes_list[];  };
layout(std430, binding=10) coherent buffer IBReturnedNodesList  { uint ib_returned_nodes_counter; uint ib_returned_nodes_list[]; };
layout(std430, binding=11) buffer EvictedChunksList { uint evicted_chunks_counter; uint evicted_chunks_list[]; };

uniform uint vb_max_order;
uniform uint ib_max_order;

// ----- include -----
#include "../utils.glsl"

#define PREFIX vb
#include "../common/allocator.glsl"

#define PREFIX ib
#include "../common/allocator.glsl"
// -------------------

void main() {
    uint envicted_list_id = gl_GlobalInvocationID.x;
    if (envicted_list_id >= evicted_chunks_counter) return;

    uint chunk_id = evicted_chunks_list[envicted_list_id];
    if (chunk_id == INVALID_ID) return;

    // Отчистка памяти
    ChunkMeshAlloc chunk_alloc = chunk_alloc_global[chunk_id];
    if (chunk_alloc.v_startPage != INVALID_ID) vb_free_pages(chunk_alloc.v_startPage, chunk_alloc.v_order);
    if (chunk_alloc.i_startPage != INVALID_ID) ib_free_pages(chunk_alloc.i_startPage, chunk_alloc.i_order);

    // Запись информации о том, что память для меша чанка chunk_id не выделенна
    chunk_alloc_global[chunk_id].v_startPage = INVALID_ID;
    chunk_alloc_global[chunk_id].v_order = 0u;
    chunk_alloc_global[chunk_id].needV = 0u;
    chunk_alloc_global[chunk_id].i_startPage = INVALID_ID;
    chunk_alloc_global[chunk_id].i_order = 0u;
    chunk_alloc_global[chunk_id].needI = 0u;
    chunk_alloc_global[chunk_id].is_valid = true;
}