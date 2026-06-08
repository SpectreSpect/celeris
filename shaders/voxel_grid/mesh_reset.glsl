#version 430
layout(local_size_x = 256) in;

layout(std430, binding=0) readonly buffer DirtyListBuf { uint dirty_count; uint dirty_list[]; };
layout(std430, binding=1) buffer DirtyQuadCountBuf { uint dirty_quad_count[]; };
layout(std430, binding=2) buffer EmitCounterBuf { uint emit_counter[]; };

void main() {
    uint dirty_idx = gl_GlobalInvocationID.x;
    if (dirty_idx >= dirty_count) return;

    dirty_quad_count[dirty_idx] = 0u;

    uint chunkId = dirty_list[dirty_idx];
    emit_counter[chunkId] = 0u;
}
