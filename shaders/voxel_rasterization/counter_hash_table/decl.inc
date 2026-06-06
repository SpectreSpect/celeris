#ifndef VOXEL_RASTERIZATION_COUNTER_HASH_TABLE_DECL
#define VOXEL_RASTERIZATION_COUNTER_HASH_TABLE_DECL

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

#endif