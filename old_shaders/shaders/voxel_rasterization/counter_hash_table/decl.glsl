#pragma once

struct CounterAllocMeta {
    uint count_triangles;
    uint triangle_indices_base;
    uint triangle_emmit_counter;
};

struct CounterHashTableSlot {
    uvec2 key;
    CounterAllocMeta value;
    uint state;
};
