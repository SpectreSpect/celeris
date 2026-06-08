#pragma once
#define SLOT_EMPTY    0xFFFFFFFFu
#define SLOT_LOCKED   0xFFFFFFFEu
#define SLOT_TOMB     0xFFFFFFFDu
#define SLOT_OCCUPIED 0xFFFFFFFCu
#define MAX_PROBES   128u
#define COUNT_TABLE_COUNTERS 16u

struct HashTableCounters {
    uint count_empty[COUNT_TABLE_COUNTERS];
    uint count_occupied[COUNT_TABLE_COUNTERS];
    uint count_tomb[COUNT_TABLE_COUNTERS];
};
