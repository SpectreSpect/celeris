#pragma once
#include "../decl.glsl"

CounterAllocMeta counter_alloc_meta_init(uvec2 key, out bool success) {
    CounterAllocMeta meta;
    meta.count_triangles = 0u;

    success = true;
    return meta;
}