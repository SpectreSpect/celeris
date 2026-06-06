void HASH_TABLE_PREFIX(CAT3(add_, COUNTER_TYPE_NAME, _counter))(uint slot_id, uint add_value) {
    uint counter_id = slot_id % COUNT_TABLE_COUNTERS;
    atomicAdd(TABLE_COUNTERS.CAT(count_, COUNTER_TYPE_NAME)[counter_id], add_value);
}

uint HASH_TABLE_PREFIX(CAT3(reduce_read_, COUNTER_TYPE_NAME, _counter))() {
    uint counter = 0u;
    for (uint i = 0u; i < COUNT_TABLE_COUNTERS; i++) {
        counter += TABLE_COUNTERS.CAT(count_, COUNTER_TYPE_NAME)[i];
    }

    return counter;
}

#undef COUNTER_TYPE_NAME

#ifndef RESET_ALL_COUNTERS_AS_EMPTY
    #define RESET_ALL_COUNTERS_AS_EMPTY 
    void reset_all_counters_as_empty() {
        uint floored_empty_count = HASH_TABLE_SIZE / COUNT_TABLE_COUNTERS;
        uint reminder = HASH_TABLE_SIZE % COUNT_TABLE_COUNTERS;

        for (uint i = 0u; i < COUNT_TABLE_COUNTERS; i++) {
            TABLE_COUNTERS.count_empty[i] = floored_empty_count + (i < reminder ? 1u : 0u);
            TABLE_COUNTERS.count_occupied[i] = 0u;    
            TABLE_COUNTERS.count_tomb[i] = 0u;
        }
    }
#endif
