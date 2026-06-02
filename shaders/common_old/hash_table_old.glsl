#include "../utils.glsl"

/*
ВАЖНО: #pragma once для этого кода не включён! Его нужно обязательно делать самим с помощью дефайнов вот таким образом:
#ifndef SOME_YOUR_PRAGMA_ONCE_DEFINE
#define SOME_YOUR_PRAGMA_ONCE_DEFINE
#include "path/to/hash_table.glsl"
#endif

===== Параметры кода =====
--- *********** Обязательные ***********
---|--- #define SLOT_VALUE_TYPE uint (тип значения, хранимого в слоте хеш-таблицы)
---|--- #define SLOT_KEY_TYPE uvec2 (тип ключа, хранимого в слоте хеш-таблицы)
---|--- 
---|--- Вместе они определяют структуру слота, из которых будет состоять хеш-таблица:
---|--- struct HashTableSlot {
---|---     SLOT_VALUE_TYPE value;
---|---     SLOT_KEY_TYPE key;
---|---     uint state;
---|--- };
---|--- **Обратите внимание, что название HashTableSlot вы задаёте проивольное и (важно)
---|--- уникальное**
---|---
---|--- #define COUNT_TOMBS count_tombs (название счётчика tomb-слотов внутри хеш-таблицы)
---|---
---|--- #define SLOTS_BUFFER slots (название буффера слотов хеш-таблицы)
---|---
---|--- Подключаемый буффер должен выглядеть следующим образом:
---|--- coherent buffer HashTable { uint COUNT_TOMBS; HashTableSlot SLOTS_BUFFER[]; };
---|---
---|--- #define KEY_HASH_FUNC hash_uvec2 (сигнатура функции - uint func(SLOT_KEY_TYPE key). Возвращает хеш-значение, соответствующие ключу)
---|---
--- *********** Не обязательные ***********
---|--- #define KEY_COMP_FUNC func (сигнатура функции - bool func(SLOT_KEY_TYPE a, SLOT_KEY_TYPE b). Сравнивает два ключа между собой. Нужна, 
---|--- если тип данных/структура не умеет сравнивать сама - такое бывает с вещественными данными).
---|---
---|--- #define VALUE_INIT_FUNC func (сигнатура функции - SLOT_VALUE_TYPE func(SLOT_KEY_TYPE key, out bool success). Инициализирует значение структуры 
---|--- SLOT_VALUE_TYPE. При отсутствии функции структура будет заполненна дефолтными значениями (если определены) или мусором).
---|---
---|--- #define INIT_SLOT_CALLBACK func (сигнатура функции - void func(uint slot_id, SLOT_KEY_TYPE key, SLOT_VALUE_TYPE value). Callback-функция, вызываемая
---|--- в момент успешной инициализации слота, но перед публикацией его состояния state).
---|---
---|--- #define TOMB_CHECK_LIST_SIZE n (размер очереди tomb-слотов, в которые будет происходит вставка в приоритете. Число n должно быть n = 2^i).
---|---
===== Параметры подключения =====
--- #define NOT_UNDEF (убирает разопределение дефайнов в конце файла)
--- 
--- #define INCLUDE_ALL_HASH_TABLE/NOT_INCLUDE_ALL_HASH_TABLE:
---|--- uniform uint u_hash_table_size;
---|--- coherent buffer HashTable { uint COUNT_TOMBS; HashTableSlot SLOTS_BUFFER[]; };
---
--- #define INCLUDE_HASH_TABLE_COMMON/NOT_INCLUDE_HASH_TABLE_COMMON
---
--- #define INCLUDE_HASH_TABLE_LOOKUP_REMOVE/NOT_INCLUDE_HASH_TABLE_LOOKUP_REMOVE:
---|--- uniform uint u_hash_table_size;
---|--- coherent buffer HashTable { uint COUNT_TOMBS; HashTableSlot SLOTS_BUFFER[]; };
---
--- #define INCLUDE_HASH_TABLE_GET_OR_CREATE/NOT_INCLUDE_HASH_TABLE_GET_OR_CREATE:
---|--- uniform uint u_hash_table_size;
---|--- coherent buffer HashTable { uint COUNT_TOMBS; HashTableSlot SLOTS_BUFFER[]; };
*/

#ifndef NOT_INCLUDE_ALL_HASH_TABLE
#define INCLUDE_ALL_HASH_TABLE
#endif

#if (defined(INCLUDE_ALL_HASH_TABLE) || defined(INCLUDE_HASH_TABLE_COMMON)) && !defined(NOT_INCLUDE_HASH_TABLE_COMMON)
#define SLOT_EMPTY    0xFFFFFFFFu
#define SLOT_LOCKED   0xFFFFFFFEu
#define SLOT_TOMB     0xFFFFFFFDu
#define SLOT_OCCUPIED 0xFFFFFFFCu
#define MAX_PROBES   128u

#ifndef TOMB_CHECK_LIST_SIZE
#define TOMB_CHECK_LIST_SIZE 32u
#endif

const uint TOMB_LIST_MASK = TOMB_CHECK_LIST_SIZE - 1u;

uint tomb_check_list[TOMB_CHECK_LIST_SIZE];
uint tomb_list_head_id = INVALID_ID;
uint tomb_list_tail_id = INVALID_ID;
uint tomb_list_count_elements = 0u;

bool push_tomb_id(uint slot_id) {
    if (tomb_list_count_elements >= TOMB_CHECK_LIST_SIZE)
        return false;

    if (tomb_list_count_elements == 0u) {
        tomb_list_head_id = 0u;
        tomb_list_tail_id = 0u;
    }
    else  {
        tomb_list_head_id = (tomb_list_head_id + 1u) & TOMB_LIST_MASK;
     }

    tomb_check_list[tomb_list_head_id] = slot_id;
    tomb_list_count_elements++;
    return true;
}

uint pop_tail_tomb_id() {
    if (tomb_list_count_elements == 0u)
        return INVALID_ID;

    uint result = tomb_check_list[tomb_list_tail_id];
    tomb_list_count_elements--;

    if (tomb_list_count_elements == 0u) {
        // Когда элементов нет, не может существовать id элемента головы и хвоста - это логически корректно.
        // В теории можно было бы обойтись другими путями, но кажется так всех проще и логичнее.
        tomb_list_head_id = INVALID_ID;
        tomb_list_tail_id = INVALID_ID;
    } else {
        tomb_list_tail_id = (tomb_list_tail_id + 1u) & TOMB_LIST_MASK;
    }

    return result;
}
#endif

#if (defined(INCLUDE_ALL_HASH_TABLE) || defined(INCLUDE_HASH_TABLE_LOOKUP_REMOVE)) && !defined(NOT_INCLUDE_HASH_TABLE_LOOKUP_REMOVE)
uint lookup_hash_table_slot_id(SLOT_KEY_TYPE key, bool read_only = false) {
    uint idx = KEY_HASH_FUNC(key) % u_hash_table_size;

    for (uint probe = 0u; probe < MAX_PROBES;) {
        uint state = read_only ? SLOTS_BUFFER[idx].state : atomicAdd(SLOTS_BUFFER[idx].state, 0u);

        if (state == SLOT_LOCKED) continue;

        if (state == SLOT_TOMB) {
            idx = (idx + 1u) % u_hash_table_size;
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
        
        idx = (idx + 1u) % u_hash_table_size;
        probe++;
    }

    return INVALID_ID;
}

bool remove_slot_from_hash_table(SLOT_KEY_TYPE key) {
    uint idx  = KEY_HASH_FUNC(key) % u_hash_table_size;

    for (uint probe = 0u; probe < MAX_PROBES;) {
        uint state = atomicAdd(SLOTS_BUFFER[idx].state, 0u);

        if (state == SLOT_LOCKED) continue;

        if (state == SLOT_EMPTY) return false;

        if (state == SLOT_TOMB) { 
            idx = (idx + 1u) % u_hash_table_size;
            probe++;
            continue;
        }

        if (state == SLOT_OCCUPIED) {
            memoryBarrierBuffer();

            #ifdef KEY_COMP_FUNC
                if (KEY_COMP_FUNC(SLOTS_BUFFER[idx].key, key)) {
                    atomicExchange(SLOTS_BUFFER[idx].state, SLOT_TOMB); // Удаляем слот
                    atomicAdd(COUNT_TOMBS, 1u);
                    return true;
                }
            #else
                if (SLOTS_BUFFER[idx].key == key) {
                    atomicExchange(SLOTS_BUFFER[idx].state, SLOT_TOMB); // Удаляем слот
                    atomicAdd(COUNT_TOMBS, 1u);
                    return true;
                }
            #endif
        }

        idx = (idx + 1u) % u_hash_table_size;
        probe++;
    }

    return false;
}

bool set_slot_value(SLOT_KEY_TYPE key, SLOT_VALUE_TYPE value) {
    uint idx  = KEY_HASH_FUNC(key) % u_hash_table_size;
    uint last_tomb_id = INVALID_ID;

    tomb_list_head_id = INVALID_ID;
    tomb_list_tail_id = INVALID_ID;
    tomb_list_count_elements = 0u;

    for (uint probe = 0u; probe < MAX_PROBES + 1u;) {
        uint state = atomicAdd(SLOTS_BUFFER[idx].state, 0u);

        if (state == SLOT_EMPTY || probe == MAX_PROBES) {
            if (probe >= MAX_PROBES) idx = INVALID_ID;

            // Не нашли элемент. Значит создаём (в приоритете в first_tomb)
            if (last_tomb_id == INVALID_ID) last_tomb_id = pop_tail_tomb_id();

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
                atomicAdd(COUNT_TOMBS, 0xFFFFFFFFu); // Вычитаем единицу через переполнение (безопасно, так как COUNT_TOMBS > 0)
            }

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
            push_tomb_id(idx);
            idx = (idx + 1u) % u_hash_table_size;
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


        idx = (idx + 1u) % u_hash_table_size;
        probe++;
    }

    return false;
}
#endif

#if (defined(INCLUDE_ALL_HASH_TABLE) || defined(INCLUDE_HASH_TABLE_GET_OR_CREATE)) && !defined(NOT_INCLUDE_HASH_TABLE_GET_OR_CREATE)
bool get_or_create_chunk(SLOT_KEY_TYPE key, out uint out_slot_id, out bool created) {
    uint idx  = KEY_HASH_FUNC(key) % u_hash_table_size;
    uint last_tomb_id = INVALID_ID;

    tomb_list_head_id = INVALID_ID;
    tomb_list_tail_id = INVALID_ID;
    tomb_list_count_elements = 0u;

    for (uint probe = 0u; probe < MAX_PROBES + 1u;) {
        uint state = atomicAdd(SLOTS_BUFFER[idx].state, 0u);

        if (state == SLOT_EMPTY || probe == MAX_PROBES) {
            if (probe >= MAX_PROBES) idx = INVALID_ID;

            // Не нашли элемент. Значит создаём (в приоритете в first_tomb)
            if (last_tomb_id == INVALID_ID) last_tomb_id = pop_tail_tomb_id();

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
                atomicAdd(COUNT_TOMBS, 0xFFFFFFFFu); // Вычитаем единицу через переполнение (безопасно, так как COUNT_TOMBS > 0)
            }

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
            push_tomb_id(idx);
            idx = (idx + 1u) % u_hash_table_size;
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
        
        idx = (idx + 1u) % u_hash_table_size;
        probe++;
    }

    out_slot_id = INVALID_ID;
    created = false;
    return false;
}
#endif

#ifndef NOT_UNDEF
    #ifdef INCLUDE_ALL_HASH_TABLE
        #undef INCLUDE_ALL_HASH_TABLE
    #endif

    #ifdef NOT_INCLUDE_ALL_HASH_TABLE
        #undef NOT_INCLUDE_ALL_HASH_TABLE
    #endif

    #ifdef INCLUDE_HASH_TABLE_COMMON
        #undef INCLUDE_HASH_TABLE_COMMON
    #endif

    #ifdef NOT_INCLUDE_HASH_TABLE_COMMON
        #undef NOT_INCLUDE_HASH_TABLE_COMMON
    #endif

    #ifdef TOMB_CHECK_LIST_SIZE
        #undef TOMB_CHECK_LIST_SIZE
    #endif

    #ifdef INCLUDE_HASH_TABLE_GET_OR_CREATE
        #undef INCLUDE_HASH_TABLE_GET_OR_CREATE
    #endif

    #ifdef NOT_INCLUDE_HASH_TABLE_GET_OR_CREATE
        #undef NOT_INCLUDE_HASH_TABLE_GET_OR_CREATE
    #endif

    #ifdef INCLUDE_HASH_TABLE_LOOKUP_REMOVE
        #undef INCLUDE_HASH_TABLE_LOOKUP_REMOVE
    #endif

    #ifdef NOT_INCLUDE_HASH_TABLE_LOOKUP_REMOVE
        #undef NOT_INCLUDE_HASH_TABLE_LOOKUP_REMOVE
    #endif

    #ifdef SLOT_VALUE_TYPE
        #undef SLOT_VALUE_TYPE
    #endif

    #ifdef SLOT_KEY_TYPE
        #undef SLOT_KEY_TYPE
    #endif

    #ifdef COUNT_TOMBS
        #undef COUNT_TOMBS
    #endif

    #ifdef SLOTS_BUFFER
        #undef SLOTS_BUFFER
    #endif

    #ifdef KEY_HASH_FUNC
        #undef KEY_HASH_FUNC
    #endif

    #ifdef KEY_COMP_FUNC
        #undef KEY_COMP_FUNC
    #endif

    #ifdef VALUE_INIT_FUNC
        #undef VALUE_INIT_FUNC
    #endif

    #ifdef INIT_SLOT_CALLBACK
        #undef INIT_SLOT_CALLBACK
    #endif
#endif