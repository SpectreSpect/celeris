#include "common.inc"
#include "push_pop_impl.inc"
#include "verify.inc"

#define P(NAME) PR(PREFIX, NAME)

uint P(alloc_pages)(uint wantOrder) {
    // Пытаемся достать страницу подходящего размера (2^wantOrder). Если страниц размера 2^wantOrder нет, 
    // то пытаемся достать страницу большего размера. Искать страницы меньшего размера не имеет смысла, потому
    // что наши данные в них не полезут (тк меньше чем 2^wantOrder), а объединить их не получится - функция
    // free_pages сразу при освобождении страниц объединяет их, а если они не объеденены, значит сделать
    // этого было невозможно (необходимые страницы для объединения заняты ST_ALLOC).

    for (uint attempt = 0u; attempt < ALLOCATION_ATTEMPTS; attempt++) {
        for (uint order = wantOrder; order <= P(max_order); order++) {
            uint start_page = P(pop_free)(order); // Пытаемся достать страницу минимального размера
            if (start_page == INVALID_ID)
                continue; // Список страниц размера order пуст. Пытаемся найти страницу на новой итерации цикла.
            
            // Состояние страницы start_page сразу самой функций pop_free() назначается как ST_ALLOC.
            // Страницей владеем только мы, поэтому можем делать с ней что захотим без атомиков.

            // Теперь нужно поделить страницу до нужного нами размера (2^wantOrder), а остальное освободить.
            for (uint entire_order = order; entire_order > wantOrder; entire_order--) {
                uint half_page_size = 1u << (entire_order - 1u);
                uint start_buddy = start_page + half_page_size;

                atomicExchange(P(state)[start_buddy], pack_state(entire_order - 1, ST_MERGED));
                bool ok = P(push_free)(entire_order - 1u, start_buddy);
                if (!ok) {
                    return INVALID_ID;
                }
            }
            
            // Теперь нужно сделать размер start_page равным wantOrder, так как он до сих пор равен order.
            // За одно поставили ST_ALLOC (тк так проще установить wantOrder), но в теории состояние и так уже 
            // должно было быть ST_ALLOC.
            atomicExchange(P(state)[start_page], pack_state(wantOrder, ST_ALLOC));
            
            return start_page;
        }
    }

    return INVALID_ID;
}

#undef P
