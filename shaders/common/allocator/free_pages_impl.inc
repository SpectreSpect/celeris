#include "common.inc"
#include "push_pop_impl.inc"
#include "verify.inc"

#define P(NAME) PR(PREFIX, NAME)

//Освобождать можно только уже выделенную память (та, что имеет состояние ST_ALLOC)!!! Иначе работать не будет!
bool P(free_pages)(uint start, uint order) {
    if (start == INVALID_ID) return false;
    if (order > P(max_order)) return false;

    // Проверка, что освобождаемая память является занятой (ST_ALLOC) и также попытка забрать владение на освобожение
    uint alloc_state = pack_state(order, ST_ALLOC);
    uint prev_input_start_state = atomicCompSwap(P(state)[start], alloc_state, pack_state(order, ST_MERGED)); 
    if (prev_input_start_state != alloc_state) {
        // Либо кто-то раньше нас начал освобождать, либо память не была ST_ALLOC
        return false;
    }

    uint merge_id = 0u;
    uint cur_start = start;
    uint cur_order = order;
    while (cur_order < P(max_order)) {
        memoryBarrierBuffer(); // На всякий случай.

        uint buddy_size = 1u << cur_order;
        uint start_buddy = cur_start ^ buddy_size;

        if (start_buddy + buddy_size > (1u << P(max_order))) break;
        
        uint buddy_state = atomicAdd(P(state)[start_buddy], 0u);

        uint buddy_kind = buddy_state & ST_MASK;
        uint buddy_order = buddy_state >> ST_MASK_BITS;

        if(buddy_kind != ST_FREE || buddy_order != cur_order) break;

        // Здесь buddy является ST_FREE
        uint free_state = pack_state(buddy_order, ST_FREE);
        if (atomicCompSwap(P(state)[start_buddy], free_state, pack_state(buddy_order, ST_MERGED)) != free_state) {
            // Кто-то первее забрал buddy
            break;
        }

        cur_start = min(cur_start, start_buddy);
        cur_order++;
        merge_id++;
    }
    
    uint state_before_change = atomicExchange(P(state)[cur_start], pack_state(cur_order, ST_MERGED));
    bool result = P(push_free)(cur_order, cur_start);
    return result;
}

#undef P
