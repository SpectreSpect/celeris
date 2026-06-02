#include "../utils.glsl"

/*
Для корректной работы кода перед #include необходимо обязательно определить:
#define PREFIX some_prefix

В противном случае будет подключена только секция ALLOCATOR_COMMON.

Также необходимо наличие следующих данных (названия обязательно должны соответсвовать указанным вместе с заменённым
PREFIX на определённое ранее вами название).

Буфферы:
coherent buffer Heads { uint PREFIX_heads[]; };
coherent buffer State { uint PREFIX_state[]; };
coherent buffer Nodes  { Node PREFIX_nodes[];  };
coherent buffer FreeNodesList  { uint PREFIX_free_nodes_counter; uint PREFIX_free_nodes_list[];  };
buffer ReturnedNodesList  { uint PREFIX_returned_nodes_counter; uint PREFIX_returned_nodes_list[]; };

Юниформы:
uniform uint PREFIX_max_order;
*/

#ifndef ALLOCATOR_COMMON
#define ALLOCATOR_COMMON

// ---- state packing ----
#define ST_MASK_BITS 4u
#define ST_FREE 0u
#define ST_ALLOC 1u
#define ST_MERGED 2u
#define ST_WAIT_TO_FREE 4u
const uint ST_MASK = (1u << ST_MASK_BITS) - 1u;

// ---- head state packing ----
#define HEAD_TAG_BITS 4u // 4 бита вроде мало, но пока ABA вроде нет
const uint HEAD_TAG_MASK = (1u << HEAD_TAG_BITS) - 1u;
const uint INVALID_HEAD_IDX = INVALID_ID >> HEAD_TAG_BITS;

uint pack_state(uint order, uint kind) { return (order << ST_MASK_BITS) | (kind & ST_MASK); };
uint pack_head(uint start, uint tag) {return (start << HEAD_TAG_BITS) | (tag & HEAD_TAG_MASK); };

#endif

#ifdef PREFIX
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

uint P(alloc_pages)(uint wantOrder) {
    // Пытаемся достать страницу подходящего размера (2^wantOrder). Если страниц размера 2^wantOrder нет, 
    // то пытаемся достать страницу большего размера. Искать страницы меньшего размера не имеет смысла, потому
    // что наши данные в них не полезут (тк меньше чем 2^wantOrder), а объединить их не получится - функция
    // free_pages сразу при освобождении страниц объединяет их, а если они не объеденены, значит сделать
    // этого было невозможно (необходимые страницы для объединения заняты ST_ALLOC).

    uint attempt = 0u;
    while (attempt < 32u) {
        // Показываем, что сейчас идёт аллокация
        // atomicAdd(alloc_maker, 1u);

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
                P(push_free)(entire_order - 1, start_buddy);

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
            P(state)[start_page] = pack_state(wantOrder, ST_ALLOC);
            return start_page;
        }

        // atomicAdd(alloc_maker, 0xFFFFFFFFu);

        // if (atomicAdd(alloc_maker, 0u) == 0u) {
        //     // Значит что ни один поток не выделяет память, но мы всё равно не смогли найти подходящий для нас размер
        //     // страницы. Такое может быть только в том случае, если память действительно закончилась.
        //     return INVALID_ID; 
        // }

        // Какой-то поток сейчас выделяет страницы, значит он может нарезать кусочки, которые нам могут подойти, поэтому имеет
        // смысл попытаться выделить память снова.
        attempt++;
    }

    return INVALID_ID;
    // Цикл бесконечный, поэтому сюда никогда не попадём
}

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
#undef PREFIX
#endif