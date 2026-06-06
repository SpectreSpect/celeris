#version 430
layout(local_size_x = 1) in;

layout(std430, binding=0) buffer VBReturnedNodesList  { uint vb_returned_nodes_counter; uint vb_returned_nodes_list[]; };
layout(std430, binding=1) buffer IBReturnedNodesList  { uint ib_returned_nodes_counter; uint ib_returned_nodes_list[]; };
layout(std430, binding=2) buffer DispatchArgs { uvec3 dispatch_args; };

// ----- include -----
#include "../utils.glsl"
// -------------------

void main() {
    if (gl_GlobalInvocationID.x != 0u) return;

    uint max_count_returned_nodes = max(vb_returned_nodes_counter, ib_returned_nodes_counter);
    uint returned_node_groups = div_up_u32(max_count_returned_nodes, 256u);
    dispatch_args = uvec3(max(returned_node_groups, 1u), 1u, 1u);
}
