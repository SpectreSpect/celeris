#version 430
layout(local_size_x = 1) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) buffer EvictedChunksList { uint evicted_chunks_counter; uint evicted_chunks_list[]; };
layout(std430, binding=1) buffer DispatchArgs { uvec3 dispatch_args; };
layout(std430, binding=2) buffer FreeList { uint free_count; uint free_list[]; };

uniform uint u_min_free;

// ----- include -----
#include "../utils.glsl"
// -------------------

void main() {
    if (gl_GlobalInvocationID.x != 0u) return;

    if (free_count < u_min_free) {
        uint count_chunks_to_envict = u_min_free - free_count;
        evicted_chunks_counter = count_chunks_to_envict;

        uint count_groups_to_envict = div_up_u32(count_chunks_to_envict, 256u);
        dispatch_args = uvec3(count_groups_to_envict, 1u, 1u);
    } else {
        evicted_chunks_counter = 0u;
        dispatch_args = uvec3(0u, 0u, 0u); // Это означает, что шейдер запускаться не будет - то что и нужно
    }
}