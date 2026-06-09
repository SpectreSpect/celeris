#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) readonly buffer VoxelsWriteDataSrc { uint count_voxel_writes_src; uint pad0_[3u]; VoxelWrite voxel_writes_src[]; };
layout(std430, binding=1) buffer VoxelsWriteDataDsc { uint count_voxel_writes_dsc; uint pad1_[3u]; VoxelWrite voxel_writes_dsc[]; };

void main() {
    uint voxel_write_src_id = gl_GlobalInvocationID.x;
    if (voxel_write_src_id >= count_voxel_writes_src) return;

    voxel_writes_dsc[count_voxel_writes_dsc + voxel_write_src_id] = voxel_writes_src[voxel_write_src_id];
}
