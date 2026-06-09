#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) readonly buffer LocalChunkMeshAllocBuf { ChunkMeshAlloc chunk_alloc_local[]; }; 
layout(std430, binding=1) buffer GlobalChunkMeshAllocBuf { ChunkMeshAlloc chunk_alloc_global[]; }; 
layout(std430, binding=2) readonly buffer DirtyListBuf { uint dirty_count; uint dirty_list[]; };
layout(std430, binding=3) buffer MeshBuffersStatusBuf { uint is_vb_full; uint is_ib_full; };
layout(std430, binding=4) coherent buffer VBHeads { uint vb_heads[]; };
layout(std430, binding=5) coherent buffer VBState { uint vb_state[]; };
layout(std430, binding=6) coherent buffer VBNodes  { Node vb_nodes[];  };
layout(std430, binding=7) coherent buffer VBFreeNodesList  { uint vb_free_nodes_counter; uint vb_free_nodes_list[];  };
layout(std430, binding=8) coherent buffer VBReturnedNodesList  { uint vb_returned_nodes_counter; uint vb_returned_nodes_list[]; };
layout(std430, binding=9) coherent buffer IBHeads { uint ib_heads[]; };
layout(std430, binding=10) coherent buffer IBState { uint ib_state[]; };
layout(std430, binding=11) coherent buffer IBNodes { Node ib_nodes[];  };
layout(std430, binding=12) coherent buffer IBFreeNodesList  { uint ib_free_nodes_counter; uint ib_free_nodes_list[];  };
layout(std430, binding=13) coherent buffer IBReturnedNodesList  { uint ib_returned_nodes_counter; uint ib_returned_nodes_list[]; };

uniform uint vb_max_order;
uniform uint ib_max_order;

// ----- include -----
#include "../utils.glsl"

#define PREFIX vb
#include "../common/allocator.glsl"

#define PREFIX ib
#include "../common/allocator.glsl"
// -------------------

void free_chunk_mesh(uint chunk_id_global) {
    ChunkMeshAlloc a = chunk_alloc_global[chunk_id_global];
    if (a.v_startPage != INVALID_ID) vb_free_pages(a.v_startPage, a.v_order);
    if (a.i_startPage != INVALID_ID) ib_free_pages(a.i_startPage, a.i_order);
}

void main() {

    uint dirtyIdx = gl_GlobalInvocationID.x;
    uint dirtyCount = dirty_count;
    if (dirtyIdx >= dirtyCount) return;

    if (is_vb_full != 1u && is_ib_full != 1u) {
        uint chunkId = dirty_list[dirtyIdx];
        free_chunk_mesh(chunkId);
        chunk_alloc_global[chunkId] = chunk_alloc_local[dirtyIdx];
    }
}
