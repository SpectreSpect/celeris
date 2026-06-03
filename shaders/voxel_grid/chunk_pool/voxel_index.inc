#ifndef VOXEL_GRID_CHUNK_POOL_VOXEL_INDEX_GLSL
#define VOXEL_GRID_CHUNK_POOL_VOXEL_INDEX_GLSL

uint voxel_index(uvec3 p) {
    return (p.z * u_chunk_dim_y + p.y) * u_chunk_dim_x + p.x;
}

#endif // VOXEL_GRID_CHUNK_POOL_VOXEL_INDEX_GLSL
