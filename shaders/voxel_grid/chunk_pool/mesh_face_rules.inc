#ifndef VOXEL_GRID_MESH_FACE_RULES
#define VOXEL_GRID_MESH_FACE_RULES

#include "chunk_pool/read_voxels.inc"

#define MESH_FACE_NONE 0u
#define MESH_FACE_VISIBLE 1u
#define MESH_FACE_INFLATED 2u

bool voxel_can_emit_faces(uint flags)
{
    if ((flags & VOXEL_VISABILITY_FLAG_BIT) != 0u) return true;
    return display_inflated_voxels != 0u &&
           (flags & VOXEL_INFLATED_BIT) != 0u;
}

uint neighbor_voxel_flags(uint chunkId, ivec3 chunkCoord, ivec3 p, ivec3 d)
{
    bool success;
    VoxelData voxel_data = neighbor_voxel_data(chunkId, chunkCoord, p, d, success);
    if (!success) return 0u;

    return read_voxel_flags(voxel_data.type_flags);
}

uint mesh_face_kind(uint current_flags, uint neighbor_flags)
{
    bool current_visible = (current_flags & VOXEL_VISABILITY_FLAG_BIT) != 0u;
    bool neighbor_visible = (neighbor_flags & VOXEL_VISABILITY_FLAG_BIT) != 0u;
    bool visible_boundary = current_visible != neighbor_visible;

    if (current_visible && visible_boundary) return MESH_FACE_VISIBLE;
    if (display_inflated_voxels == 0u) return MESH_FACE_NONE;

    bool current_inflated = (current_flags & VOXEL_INFLATED_BIT) != 0u;
    bool neighbor_inflated = (neighbor_flags & VOXEL_INFLATED_BIT) != 0u;
    bool inflated_boundary = current_inflated != neighbor_inflated;

    if (current_inflated && inflated_boundary && !visible_boundary) {
        return MESH_FACE_INFLATED;
    }

    return MESH_FACE_NONE;
}

#endif
