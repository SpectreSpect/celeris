#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) buffer Voxels { VoxelData voxels[]; };
layout(std430, binding=1) readonly buffer IdsToClear { uint ids_to_clear[]; };
layout(std430, binding=2) readonly buffer VoxelPrifab { VoxelData voxel_prifab; };

uniform uint u_count_chunks_to_clear;
uniform uint u_chunk_size;

void main(){
    uint voxel_id = gl_GlobalInvocationID.x;
    uint list_idx = gl_GlobalInvocationID.y;

    if (list_idx >= u_count_chunks_to_clear) return;

    uint chunk_id = ids_to_clear[list_idx];
    voxels[chunk_id * u_chunk_size + voxel_id] = voxel_prifab;
}
