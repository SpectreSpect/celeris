#include "../../utils.glsl"
#include "hash_table_decl.glsl"

#define HASH_TABLE_PREFIX(NAME) PR(HT_PREFIX, NAME)

#ifndef TOMB_CHECK_LIST_SIZE
#define TOMB_CHECK_LIST_SIZE 32u
#endif

const uint HASH_TABLE_PREFIX(TOMB_LIST_MASK) = TOMB_CHECK_LIST_SIZE - 1u;

uint HASH_TABLE_PREFIX(tomb_check_list)[TOMB_CHECK_LIST_SIZE];
uint HASH_TABLE_PREFIX(tomb_list_head_id) = INVALID_ID;
uint HASH_TABLE_PREFIX(tomb_list_tail_id) = INVALID_ID;
uint HASH_TABLE_PREFIX(tomb_list_count_elements) = 0u;

#define COUNTER_TYPE_NAME empty
#include "hash_table_counters_impl.glsl"

#define COUNTER_TYPE_NAME occupied
#include "hash_table_counters_impl.glsl"

#define COUNTER_TYPE_NAME tomb
#include "hash_table_counters_impl.glsl"

bool HASH_TABLE_PREFIX(push_tomb_id)(uint slot_id) {
    if (HASH_TABLE_PREFIX(tomb_list_count_elements) >= TOMB_CHECK_LIST_SIZE)
        return false;

    if (HASH_TABLE_PREFIX(tomb_list_count_elements) == 0u) {
        HASH_TABLE_PREFIX(tomb_list_head_id) = 0u;
        HASH_TABLE_PREFIX(tomb_list_tail_id) = 0u;
    }
    else  {
        HASH_TABLE_PREFIX(tomb_list_head_id) = (HASH_TABLE_PREFIX(tomb_list_head_id) + 1u) & HASH_TABLE_PREFIX(TOMB_LIST_MASK);
     }

    HASH_TABLE_PREFIX(tomb_check_list)[HASH_TABLE_PREFIX(tomb_list_head_id)] = slot_id;
    HASH_TABLE_PREFIX(tomb_list_count_elements)++;
    return true;
}

uint HASH_TABLE_PREFIX(pop_tail_tomb_id)() {
    if (HASH_TABLE_PREFIX(tomb_list_count_elements) == 0u)
        return INVALID_ID;

    uint result = HASH_TABLE_PREFIX(tomb_check_list)[HASH_TABLE_PREFIX(tomb_list_tail_id)];
    HASH_TABLE_PREFIX(tomb_list_count_elements)--;

    if (HASH_TABLE_PREFIX(tomb_list_count_elements) == 0u) {
        // Когда элементов нет, не может существовать id элемента головы и хвоста - это логически корректно.
        // В теории можно было бы обойтись другими путями, но кажется так всех проще и логичнее.
        HASH_TABLE_PREFIX(tomb_list_head_id) = INVALID_ID;
        HASH_TABLE_PREFIX(tomb_list_tail_id) = INVALID_ID;
    } else {
        HASH_TABLE_PREFIX(tomb_list_tail_id) = (HASH_TABLE_PREFIX(tomb_list_tail_id) + 1u) & HASH_TABLE_PREFIX(TOMB_LIST_MASK);
    }

    return result;
}
