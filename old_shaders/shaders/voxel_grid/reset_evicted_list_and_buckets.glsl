#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) coherent buffer BucketHeads { BucketHead bucket_heads[]; };
layout(std430, binding=1) buffer EvictedChunksList { uint evicted_chunks_counter; uint evicted_chunks_list[]; };
layout(std430, binding=2) buffer FreeList { uint free_count; uint free_list[]; };

uniform uint u_bucket_count;

void main() {
    if (gl_GlobalInvocationID.x == 0u) {
        free_count += evicted_chunks_counter;
        evicted_chunks_counter = 0u;
    }

    uint bucket_id = gl_GlobalInvocationID.x;
    if (bucket_id >= u_bucket_count) return;

    bucket_heads[bucket_id].count = 0u;
}