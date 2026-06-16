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

    uint zero_attempts = 0u;

    while (true) {
        // Показываем, что сейчас идёт аллокация
        // atomicAdd(alloc_maker, 1u);

        for (uint order = wantOrder; order <= P(max_order); order++) {
            uint start_page = P(pop_free)(order); // Пытаемся достать страницу минимального размера
            if (start_page == INVALID_ID)
                continue; // Список страниц размера order пуст. Пытаемся найти страницу на новой итерации цикла.
            
            bool will_split = order > wantOrder;
            if (will_split) {
                atomicAdd(P(active_splitters), 1u);
            }

            // Состояние страницы start_page сразу самой функций pop_free() назначается как ST_ALLOC.
            // Страницей владеем только мы, поэтому можем делать с ней что захотим без атомиков.

            // Теперь нужно поделить страницу до нужного нами размера (2^wantOrder), а остальное освободить.
            for (uint entire_order = order; entire_order > wantOrder; entire_order--) {
                uint half_page_size = 1u << (entire_order - 1u);
                uint start_buddy = start_page + half_page_size;

                atomicExchange(P(state)[start_buddy], pack_state(entire_order - 1, ST_MERGED));
                bool ok = P(push_free)(entire_order - 1u, start_buddy);
                if (!ok) {
                    atomicAdd(P(active_splitters), 0xFFFFFFFFu);
                    return INVALID_ID;
                }

                // // Освобождаем страницу start_buddy
                // if (!push_free(entire_order - 1, start_buddy)) {
                //     // Страницу вставил кто-то до нас. Но такого в теории быть не должно, потому что страницей 
                //     // владеем только мы (это обеспечивается функцией pop_free).
                //     atomicAdd(alloc_maker, 0xFFFFFFFF); // Отнимаем единицу с помощью переполнения
                //     return INVALID_ID;
                // }
            }

            // Показываем, что аллокация завершена
            // atomicAdd(alloc_maker, 0xFFFFFFFF);
            
            // Теперь нужно сделать размер start_page равным wantOrder, так как он до сих пор равен order.
            // За одно поставили ST_ALLOC (тк так проще установить wantOrder), но в теории состояние и так уже 
            // должно было быть ST_ALLOC.
            atomicExchange(P(state)[start_page], pack_state(wantOrder, ST_ALLOC));
            
            if (will_split) {
                atomicAdd(P(active_splitters), 0xFFFFFFFFu);
            }
            
            return start_page;
        }

        uint splitters = atomicAdd(P(active_splitters), 0u);

        if (splitters == 0u) {
            zero_attempts++;
            if (zero_attempts >= ZERO_ATTEMPTS) return INVALID_ID;
        } else {
            zero_attempts = 0u;
        }

        // atomicAdd(alloc_maker, 0xFFFFFFFFu);

        // if (atomicAdd(alloc_maker, 0u) == 0u) {
        //     // Значит что ни один поток не выделяет память, но мы всё равно не смогли найти подходящий для нас размер
        //     // страницы. Такое может быть только в том случае, если память действительно закончилась.
        //     return INVALID_ID; 
        // }
    }

    return INVALID_ID;
    // Цикл бесконечный, поэтому сюда никогда не попадём
}

#undef P