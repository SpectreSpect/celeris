#ifndef COUNTER_HASH_TABLE_ALLOC_META_INIT
#define COUNTER_HASH_TABLE_ALLOC_META_INIT

#include "../decl.inc"

CounterAllocMeta counter_alloc_meta_init(uvec2 key, out bool success) {
    CounterAllocMeta meta;
    meta.count_triangles = 0u;

    success = true;
    return meta;
}

#endif