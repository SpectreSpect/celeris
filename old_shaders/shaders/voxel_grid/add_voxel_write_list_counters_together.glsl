#version 430
layout(local_size_x = 1) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) readonly buffer VoxelsWriteDataSrc { uint count_voxel_writes_src; uint pad0_[3u]; VoxelWrite voxel_writes_src[]; };
layout(std430, binding=1) buffer VoxelsWriteDataDsc { uint count_voxel_writes_dsc; uint pad1_[3u]; VoxelWrite voxel_writes_dsc[]; };

void main() {
    if (gl_GlobalInvocationID.x != 0u) return;

    count_voxel_writes_dsc += count_voxel_writes_src;
}