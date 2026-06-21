#include "common.inc"
#include "nodes_impl.inc"
#include "verify.inc"

#define P(NAME) PR(PREFIX, NAME)

// При вставке состояние страницы не должно быть ST_FREE!!! Функция сама пометит страницу свободной.
bool P(push_free)(uint order, uint startPage)
{
    if (startPage == INVALID_ID) return false;

    uint node_id = P(pop_free_node_id)();
    if (node_id == INVALID_ID) return false;
    
    P(nodes)[node_id].page = startPage;

    atomicExchange(P(state)[startPage], pack_state(order, ST_FREE));

    for (;;)
    {
        uint old_h = atomicAdd(P(heads)[order], 0u);

        uint old_tag = old_h & HEAD_TAG_MASK;
        uint old_idx = old_h >> HEAD_TAG_BITS;

        P(nodes)[node_id].next = old_idx == INVALID_HEAD_IDX ? INVALID_ID : old_idx;

        memoryBarrierBuffer();

        uint new_tag = (old_tag + 1u) & HEAD_TAG_MASK;
        uint new_head = pack_head(node_id, new_tag);

        if (atomicCompSwap(P(heads)[order], old_h, new_head) == old_h)
            return true;
    }
}

uint P(pop_free)(uint order) {
    for (;;) {
        uint old_h = atomicAdd(P(heads)[order], 0u);

        uint old_tag = old_h & HEAD_TAG_MASK;
        uint old_idx = old_h >> HEAD_TAG_BITS;

        if (old_idx == INVALID_HEAD_IDX)
            return INVALID_ID; // Список пустой - выходим

        memoryBarrierBuffer(); // Чтобы old_h было ДО next

        uint next = atomicAdd(P(nodes)[old_idx].next, 0u);
        uint new_head_idx = next == INVALID_ID ? INVALID_HEAD_IDX : next;
        uint new_head_tag = (old_tag + 1u) & HEAD_TAG_MASK;
        uint new_head = pack_head(new_head_idx, new_head_tag);

        if (atomicCompSwap(P(heads)[order], old_h, new_head) != old_h) continue;

        memoryBarrierBuffer();
        
        uint old_head_page = P(nodes)[old_idx].page; // Нод доступен только нам, поэтому, в теории, чтение безопасно

        P(push_free_node_id)(old_idx);

        uint free_state = pack_state(order, ST_FREE);
        if (atomicCompSwap(P(state)[old_head_page], free_state, pack_state(order, ST_ALLOC)) == free_state) {
            return old_head_page;
        }

        

        // Если мы оказались здесь, значит страница была протухшей (ST_MERGED).
        // Мы её уже вынули из списка, поэтому всё норм - просто повторяем арбитраж снова.
    }

    // Цикл бесконечный, поэтому сюда никогда не попадём - арбитраж обязательно должен пройти успешно
    return INVALID_ID;
}

#undef P
