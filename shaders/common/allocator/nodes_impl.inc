#include "common.inc"
#include "verify.inc"

#define P(NAME) PR(PREFIX, NAME)

uint P(pop_free_node_id)() {
    for (;;) {
        uint old_counter = atomicAdd(P(free_nodes_counter), 0u);
        if (old_counter == 0u) return INVALID_ID;
        
        if (atomicCompSwap(P(free_nodes_counter), old_counter, old_counter - 1u) == old_counter) {
            memoryBarrierBuffer();
            return P(free_nodes_list)[old_counter - 1u];
        }
    }
}

void P(push_free_node_id)(uint node_id) {
    uint free_idx = atomicAdd(P(returned_nodes_counter), 1u);
    P(returned_nodes_list)[free_idx] = node_id;
}

#undef P
