#version 430
layout(local_size_x = 1) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) buffer DirtyListBuf { uint dirty_count; uint dirty_list[]; };
layout(std430, binding=1) buffer VBFreeNodesList  { uint vb_free_nodes_counter; uint vb_free_nodes_list[];  };
layout(std430, binding=2) buffer VBReturnedNodesList  { uint vb_returned_nodes_counter; uint vb_returned_nodes_list[]; };
layout(std430, binding=3) buffer IBFreeNodesList  { uint ib_free_nodes_counter; uint ib_free_nodes_list[];  };
layout(std430, binding=4) buffer IBReturnedNodesList  { uint ib_returned_nodes_counter; uint ib_returned_nodes_list[]; };

void main() {
    dirty_count = 0u;

    vb_free_nodes_counter += vb_returned_nodes_counter;
    ib_free_nodes_counter += ib_returned_nodes_counter;

    vb_returned_nodes_counter = 0u;
    ib_returned_nodes_counter = 0u;
}
