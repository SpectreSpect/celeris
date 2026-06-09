#pragma once

uint voxel_index(ivec3 p) {
    return uint((p.z * u_chunk_dim.y + p.y) * u_chunk_dim.x + p.x);
}
