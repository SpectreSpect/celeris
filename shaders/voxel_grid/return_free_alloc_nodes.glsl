#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) buffer VBFreeNodesList  { uint vb_free_nodes_counter; uint vb_free_nodes_list[];  };
layout(std430, binding=1) buffer VBReturnedNodesList  { uint vb_returned_nodes_counter; uint vb_returned_nodes_list[]; };
layout(std430, binding=2) buffer IBFreeNodesList  { uint ib_free_nodes_counter; uint ib_free_nodes_list[];  };
layout(std430, binding=3) buffer IBReturnedNodesList  { uint ib_returned_nodes_counter; uint ib_returned_nodes_list[]; };

uniform uvec3 u3_chunk_size;

// ----- include -----
#include "../utils.glsl"
// -------------------

void main() {
    uint returned_list_id = gl_GlobalInvocationID.x;

    if (returned_list_id < vb_returned_nodes_counter) {
        uint returned_node_id = vb_returned_nodes_list[returned_list_id];
        vb_free_nodes_list[vb_free_nodes_counter + returned_list_id] = returned_node_id;
    }

    if (returned_list_id < ib_returned_nodes_counter) {
        uint returned_node_id = ib_returned_nodes_list[returned_list_id];
        ib_free_nodes_list[ib_free_nodes_counter + returned_list_id] = returned_node_id;
    }
}
