#version 430
layout(local_size_x = 1) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) buffer VBHeads { uint vb_heads[]; };
layout(std430, binding=1) buffer VBNodes  { Node vb_nodes[];  };
layout(std430, binding=2) buffer VBState { uint vb_state[]; };
layout(std430, binding=3) buffer VBFreeNodesList  { uint vb_free_nodes_counter; uint vb_free_nodes_list[];  };
layout(std430, binding=4) buffer IBHeads { uint ib_heads[]; };
layout(std430, binding=5) buffer IBNodes  { Node ib_nodes[];  };
layout(std430, binding=6) buffer IBState { uint ib_state[]; };
layout(std430, binding=7) buffer IBFreeNodesList  { uint ib_free_nodes_counter; uint ib_free_nodes_list[];  };

uniform uint u_vb_max_order;
uniform uint u_ib_max_order;

// ----- include -----
#include "../utils.glsl"
#include "../common/allocator.glsl"
// -------------------

void main() {
    uint vb_node_id = vb_free_nodes_counter - 1u;
    vb_free_nodes_counter -= 1u;

    vb_nodes[vb_node_id].page = 0u;
    vb_nodes[vb_node_id].next = INVALID_ID;
    vb_heads[u_vb_max_order] = pack_head(vb_node_id, 0u);
    vb_state[0u] = pack_state(u_vb_max_order, ST_FREE);

    uint ib_node_id = ib_free_nodes_counter - 1u;
    ib_free_nodes_counter -= 1u;

    ib_nodes[ib_node_id].page = 0u;
    ib_nodes[ib_node_id].next = INVALID_ID;
    ib_heads[u_ib_max_order] = pack_head(ib_node_id, 0u);
    ib_state[0u] = pack_state(u_ib_max_order, ST_FREE);
}
