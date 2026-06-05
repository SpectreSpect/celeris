#ifndef VOXEL_GRID_CHUNK_POOL_VOXEL_INDEX_GLSL
#define VOXEL_GRID_CHUNK_POOL_VOXEL_INDEX_GLSL

uint voxel_index(ivec3 p) {
    return uint((p.z * u_chunk_dim.y + p.y) * u_chunk_dim.x + p.x);
}

#endif // VOXEL_GRID_CHUNK_POOL_VOXEL_INDEX_GLSL
