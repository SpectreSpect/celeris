#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) readonly buffer ChunkMetaBuf { ChunkMeta meta[]; };
layout(std430, binding=1) buffer ChunkMeshAllocBuf { ChunkMeshAlloc chunk_mesh_alloc[]; }; 
layout(std430, binding=2) buffer IndirectCmdBuf { uint cmd_count; DrawElementsIndirectCommand cmds[]; };

uniform uint  u_max_chunks;

// для AABB/sphere размеров чанка
uniform ivec3 u_chunk_dim;     // (16,16,16)
uniform vec3  u_voxel_size;    // (sx,sy,sz)

// pack/unpack как в C++
uniform uint u_pack_bits;
uniform int  u_pack_offset;

uniform uint u_vb_page_verts;
uniform uint u_ib_page_inds;

uniform vec3 cam_pos;
uniform float render_distance;

// 6 плоскостей фрустума в world space: ax+by+cz+d >= 0 (внутри)
uniform vec4 u_frustum_planes[6];

// ----- include -----
#include "../utils.glsl"
// -------------------

// Быстрый тест сферы против плоскостей (false positives допустимы)
bool sphere_in_frustum(vec3 center, float radius) {
    for (int i = 0; i < 6; ++i) {
        vec4 p = u_frustum_planes[i];
        float dist = dot(p.xyz, center) + p.w;
        if (dist < -radius) return false;
    }
    return true;
}

void main() {
    uint chunkId = gl_GlobalInvocationID.x;
    if (chunkId >= u_max_chunks) return;

    if (meta[chunkId].used == 0u) return;

    ChunkMeshAlloc mesh_alloc = chunk_mesh_alloc[chunkId];

    if (mesh_alloc.v_startPage == INVALID_ID || mesh_alloc.i_startPage == INVALID_ID) return;
    if (mesh_alloc.needV == 0 || mesh_alloc.needI == 0) return;


    ivec3 chunkCoord = unpack_key_to_coord(uvec2(meta[chunkId].key_lo, meta[chunkId].key_hi), u_pack_offset, u_pack_bits);

    vec3 chunkSize = vec3(u_chunk_dim) * u_voxel_size;

    vec3 minP = vec3(chunkCoord * u_chunk_dim) * u_voxel_size;
    vec3 center = minP + 0.5 * chunkSize;

    vec3 diff = center - cam_pos;
    float distance_to_chunk_2 = dot(diff, diff);
    if (distance_to_chunk_2 > render_distance * render_distance) return;

    float radius = length(chunkSize) * 0.5;

    if (!sphere_in_frustum(center, radius)) return;

    uint cmdIdx = atomicAdd(cmd_count, 1u);

    cmds[cmdIdx].count         = mesh_alloc.needI;
    cmds[cmdIdx].instanceCount = 1u;
    cmds[cmdIdx].firstIndex    = mesh_alloc.i_startPage * u_ib_page_inds;
    cmds[cmdIdx].baseVertex    = 0;
    cmds[cmdIdx].baseInstance  = chunkId;
}
