#pragma once
#include "hash_table/hash_table_decl.glsl"
#include "../voxel_grid/chunk_hash_table/decl.glsl"
#include "../voxel_rasterization/counter_hash_table/decl.glsl"

#define DIRTY_FLAG_BIT 1u
#define NEED_GENERATION_FLAG_BIT 2u
#define HAS_WRITTEN_VOXELS_BIT 4u

struct ChunkMeta { 
    uint used; 
    uint key_lo; 
    uint key_hi; 
    uint dirty_flags; 
};

struct ChunkMeshAlloc {
    uint v_startPage;
    uint v_order;
    uint needV;
    uint i_startPage;
    uint i_order;
    uint needI;
    bool is_valid;
};

struct Node {
    uint page;
    uint next;
};

#define VOXEL_TYPE_BITS 16u
const uint VOXEL_TYPE_MASK = (1u << VOXEL_TYPE_BITS) - 1u;

#define VOXEL_VISABILITY_FLAG_BIT 1u // Определяет, видим ли воксель
#define VOXEL_EASY_OVERWRITE_FLAG_BIT 2u // Определяет, можно ли заменять воксель как будто бы он "воздух" или "вода" в майне

uint read_voxel_type(uint type_flags) {
    return type_flags & VOXEL_TYPE_MASK;
}

uint read_voxel_flags(uint type_flags) {
    return type_flags >> VOXEL_TYPE_BITS;
}

uint pack_voxel_type_flags(uint type, uint flags) {
    return (flags << VOXEL_TYPE_BITS) | (type & VOXEL_TYPE_MASK);
}

struct VoxelData {
    uint type_flags; // 32u...flags|type...0u
    uint color; // 32u...R|G|B|A...0u (получается "развёрнуто" в порядке аргументов, но "логично" в hex представлении 0xRRGGBBAAu)
};

#define OVERWRITE_BIT 1u

struct VoxelWrite {
    ivec4 world_voxel; // xyz use, w unused
    VoxelData voxel_data;
    uint set_flags;
    uint _pad1;
};

struct Vertex {
    vec4 pos;
    uint color;
    uint face;
    uint pad0;
    uint pad1;
};

struct DrawElementsIndirectCommand {
    uint count;
    uint instanceCount;
    uint firstIndex;
    int  baseVertex;
    uint baseInstance;
};

struct BucketHead {
    uint id;
    uint count;
};
