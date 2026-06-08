#include "hash_table_decl.glsl"

bool HASH_TABLE_PREFIX(get_or_create_slot)(SLOT_KEY_TYPE key, out uint out_slot_id, out bool created) {
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

            if (idx_to_create == INVALID_ID) {
                out_slot_id = INVALID_ID;
                created = false;
                return false; // Нет ни SLOT_EMPTY, ни SLOT_TOMB в очереди
            }

            uint prev = atomicCompSwap(SLOTS_BUFFER[idx_to_create].state, slot_state, SLOT_LOCKED);
            if (prev != slot_state) {
                // кто-то успел — перепроверяем этот же idx
                if (prev == SLOT_LOCKED) continue;

                if (slot_state == SLOT_TOMB && prev == SLOT_OCCUPIED) {
                    memoryBarrierBuffer();

                    #ifdef KEY_COMP_FUNC
                        if (KEY_COMP_FUNC(SLOTS_BUFFER[idx_to_create].key, key)) {
                            out_slot_id = idx_to_create;
                            created = false;
                            return true;
                        }
                    #else
                        if (SLOTS_BUFFER[idx_to_create].key == key) {
                            out_slot_id = idx_to_create;
                            created = false;
                            return true;
                        }
                    #endif
                    
                    last_tomb_id = INVALID_ID;
                }

                continue;
            }

            SLOT_VALUE_TYPE value;

            #ifdef VALUE_INIT_FUNC
                bool success = false;
                value = VALUE_INIT_FUNC(key, success);
                if (!success) {
                    atomicExchange(SLOTS_BUFFER[idx_to_create].state, slot_state);
                    out_slot_id = INVALID_ID;
                    created = false;
                    return false;
                }
            #endif

            if (slot_state == SLOT_TOMB) {                
                HASH_TABLE_PREFIX(add_tomb_counter)(idx_to_create, 0xFFFFFFFFu);
            } else {
                HASH_TABLE_PREFIX(add_empty_counter)(idx_to_create, 0xFFFFFFFFu);
            }

            HASH_TABLE_PREFIX(add_occupied_counter)(idx_to_create, 1u);

            // публикуем key и value
            SLOTS_BUFFER[idx_to_create].key = key;
            SLOTS_BUFFER[idx_to_create].value = value;

            #ifdef INIT_SLOT_CALLBACK
                INIT_SLOT_CALLBACK(idx_to_create, key, value);
            #endif

            // гарантируем, что key/value видимы до публикации id
            memoryBarrierBuffer();

            // публикуем id (и одновременно "анлочим")
            atomicExchange(SLOTS_BUFFER[idx_to_create].state, SLOT_OCCUPIED);

            out_slot_id = idx_to_create;
            created = true;
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
                if (KEY_COMP_FUNC(SLOTS_BUFFER[idx].key, key)) {
                    out_slot_id = idx;
                    created = false;
                    return true;
                }
            #else
                if (SLOTS_BUFFER[idx].key == key) {
                    out_slot_id = idx;
                    created = false;
                    return true;
                }
            #endif
        }
        
        idx = (idx + 1u) % HASH_TABLE_SIZE;
        probe++;
    }

    out_slot_id = INVALID_ID;
    created = false;
    return false;
}
