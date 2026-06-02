#include "hash_table_decl.glsl"

uint HASH_TABLE_PREFIX(lookup_hash_table_slot_id)(SLOT_KEY_TYPE key, bool read_only = false) {
    uint idx = KEY_HASH_FUNC(key) % HASH_TABLE_SIZE;

    for (uint probe = 0u; probe < MAX_PROBES;) {
        uint state = read_only ? SLOTS_BUFFER[idx].state : atomicAdd(SLOTS_BUFFER[idx].state, 0u);

        if (state == SLOT_LOCKED) continue;

        if (state == SLOT_TOMB) {
            idx = (idx + 1u) % HASH_TABLE_SIZE;
            probe++;
            continue;
        }

        if (state == SLOT_EMPTY) return INVALID_ID;

        if (state == SLOT_OCCUPIED) {
            if (!read_only)
                memoryBarrierBuffer();
            
            #ifdef KEY_COMP_FUNC
                if (KEY_COMP_FUNC(SLOTS_BUFFER[idx].key, key))
                    return idx;
            #else
                if (SLOTS_BUFFER[idx].key == key) 
                    return idx;
            #endif
        }
        
        idx = (idx + 1u) % HASH_TABLE_SIZE;
        probe++;
    }

    return INVALID_ID;
}

bool HASH_TABLE_PREFIX(remove_slot_from_hash_table)(SLOT_KEY_TYPE key) {
    uint idx  = KEY_HASH_FUNC(key) % HASH_TABLE_SIZE;

    for (uint probe = 0u; probe < MAX_PROBES;) {
        uint state = atomicAdd(SLOTS_BUFFER[idx].state, 0u);

        if (state == SLOT_LOCKED) continue;

        if (state == SLOT_EMPTY) return false;

        if (state == SLOT_TOMB) { 
            idx = (idx + 1u) % HASH_TABLE_SIZE;
            probe++;
            continue;
        }

        if (state == SLOT_OCCUPIED) {
            memoryBarrierBuffer();

            #ifdef KEY_COMP_FUNC
                if (KEY_COMP_FUNC(SLOTS_BUFFER[idx].key, key)) {
                    // atomicExchange(SLOTS_BUFFER[idx].state, SLOT_TOMB);
                    // atomicAdd(TABLE_COUNTERS.count_tomb, 1u);
                    // return true;

                    uint prev = atomicCompSwap(SLOTS_BUFFER[idx].state, SLOT_OCCUPIED, SLOT_TOMB);
                    if (prev == SLOT_OCCUPIED) { // Удаляем слот
                        HASH_TABLE_PREFIX(add_tomb_counter)(idx, 1u);
                        HASH_TABLE_PREFIX(add_occupied_counter)(idx, 0xFFFFFFFFu);
                        return true;
                    } else if (prev == SLOT_TOMB) return true;
                    else continue;
                }
            #else
                if (SLOTS_BUFFER[idx].key == key) {
                    uint prev = atomicCompSwap(SLOTS_BUFFER[idx].state, SLOT_OCCUPIED, SLOT_TOMB);
                    if (prev == SLOT_OCCUPIED) { // Удаляем слот
                        HASH_TABLE_PREFIX(add_tomb_counter)(idx, 1u);
                        HASH_TABLE_PREFIX(add_occupied_counter)(idx, 0xFFFFFFFFu);
                        return true;
                    } else if (prev == SLOT_TOMB) return true;
                    else continue;

                    // atomicExchange(SLOTS_BUFFER[idx].state, SLOT_TOMB);
                    // atomicAdd(TABLE_COUNTERS.count_tomb, 1u);
                    // return true;
                }
            #endif
        }

        idx = (idx + 1u) % HASH_TABLE_SIZE;
        probe++;
    }

    return false;
}

bool HASH_TABLE_PREFIX(set_slot_value)(SLOT_KEY_TYPE key, SLOT_VALUE_TYPE value) {
    uint idx = KEY_HASH_FUNC(key) % HASH_TABLE_SIZE;
    uint last_tomb_id = INVALID_ID;

    HASH_TABLE_PREFIX(tomb_list_head_id) = INVALID_ID;
    HASH_TABLE_PREFIX(tomb_list_tail_id) = INVALID_ID;
    HASH_TABLE_PREFIX(tomb_list_count_elements) = 0u;

    for (uint probe = 0u; probe < MAX_PROBES + 1u;) {
        uint state = atomicAdd(SLOTS_BUFFER[idx].state, 0u);

        if (state == SLOT_EMPTY || probe == MAX_PROBES) {
            if (probe >= MAX_PROBES) idx = INVALID_ID;

            // Не нашли элемент. Значит создаём (в приоритете в first_tomb)
            if (last_tomb_id == INVALID_ID) last_tomb_id = HASH_TABLE_PREFIX(pop_tail_tomb_id)();

            uint idx_to_create = last_tomb_id == INVALID_ID ? idx : last_tomb_id;
            uint slot_state = last_tomb_id == INVALID_ID ? SLOT_EMPTY : SLOT_TOMB;

            if (idx_to_create == INVALID_ID) return false; // Нет ни SLOT_EMPTY, ни SLOT_TOMB в очереди

            uint prev = atomicCompSwap(SLOTS_BUFFER[idx_to_create].state, slot_state, SLOT_LOCKED);
            if (prev != slot_state) {
                // кто-то успел — перепроверяем этот же idx
                if (prev == SLOT_LOCKED) continue;

                if (slot_state == SLOT_TOMB && prev == SLOT_OCCUPIED) {
                    // Условие говорит, что мы проверяем tomb_check_list_counter - 1u слот и он оказался занят

                    memoryBarrierBuffer();
                    #ifdef KEY_COMP_FUNC
                    if (KEY_COMP_FUNC(SLOTS_BUFFER[idx_to_create].key, key))
                        return false;
                    #else
                    if (SLOTS_BUFFER[idx_to_create].key == key)
                        return false;
                    #endif
                    
                    last_tomb_id = INVALID_ID;
                }

                continue;
            }

            if (slot_state == SLOT_TOMB) {
                HASH_TABLE_PREFIX(add_tomb_counter)(idx_to_create, 0xFFFFFFFFu);
            } else {
                HASH_TABLE_PREFIX(add_empty_counter)(idx_to_create, 0xFFFFFFFFu);
            }

            HASH_TABLE_PREFIX(add_occupied_counter)(idx_to_create, 1u);

            // публикуем key и value
            SLOTS_BUFFER[idx_to_create].key = key;
            SLOTS_BUFFER[idx_to_create].value = value;

            // гарантируем, что key/value видимы до публикации id
            memoryBarrierBuffer();

            // публикуем состояние (и одновременно "анлочим")
            atomicExchange(SLOTS_BUFFER[idx_to_create].state, SLOT_OCCUPIED);

            return true;
        }

        if (state == SLOT_LOCKED) continue;

        if (state == SLOT_TOMB) {
            HASH_TABLE_PREFIX(push_tomb_id)(idx);
            idx = (idx + 1u) % HASH_TABLE_SIZE;
            probe++;
            continue;
        }

        if (state == SLOT_OCCUPIED) {
            memoryBarrierBuffer();

            #ifdef KEY_COMP_FUNC
                if (KEY_COMP_FUNC(SLOTS_BUFFER[idx].key, key))
                    return false;
            #else
                if (SLOTS_BUFFER[idx].key == key)
                    return false;
            #endif
        }


        idx = (idx + 1u) % HASH_TABLE_SIZE;
        probe++;
    }

    return false;
}
