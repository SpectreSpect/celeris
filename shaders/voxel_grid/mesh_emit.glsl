#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) coherent buffer ChunkHashTable { HashTableCounters chunk_hash_table_counters; ChunkHashTableSlot chunk_hash_table_slots[]; };
layout(std430, binding=1) readonly buffer ChunkVoxels { VoxelData voxels[]; };
layout(std430, binding=2) buffer MeshBuffersStatusBuf { uint is_vb_full; uint is_ib_full; };
layout(std430, binding=3) readonly buffer DirtyListBuf { uint dirty_count; uint dirty_list[]; };
layout(std430, binding=4) buffer EmitCounterBuf { uint emit_counter[]; };
layout(std430, binding=5) buffer ChunkMeshAllocBuf { ChunkMeshAlloc chunk_alloc[]; }; 
layout(std430, binding=6) readonly buffer ChunkMetaBuf { ChunkMeta meta[]; };
layout(std430, binding=7) buffer GlobalVB { Vertex vb[]; };
layout(std430, binding=8) buffer GlobalIB { uint ib[]; };

// ===== uniforms =====
uniform uint  u_chunk_hash_table_size;
uniform ivec3 u_chunk_dim;
uniform uint  u_voxels_per_chunk;
uniform vec3  u_voxel_size;

uniform uint u_pack_bits;
uniform int  u_pack_offset;

uniform uint u_vb_page_verts;
uniform uint u_ib_page_inds;

// ----- include -----
#include "../utils.glsl"
#include "chunk_pool/read_voxels.glsl"
// -------------------

#define AO_STEP   0.22   // сила затемнения за "ступень" 0..3
#define AO_MIN    0.35   // минимальная яркость в углу

// ===== voxel helpers =====

uint pack_ao_in_alpha(uint rgb, uint occl) {
    float ao = 1.0 - float(occl) * AO_STEP;
    ao = clamp(ao, AO_MIN, 1.0);
    uint a = uint(ao * 255.0 + 0.5);
    return (rgb & 0xFFFFFF00u) | a;
}

// возвращает occl 0..3 (0 = светло, 3 = темно)
uint ao_corner(uint chunkId, ivec3 chunkCoord, ivec3 p,
               ivec3 N, ivec3 U, ivec3 V, int su, int sv)
{
    ivec3 du = U * su;
    ivec3 dv = V * sv;

    uint s1 = voxel_vis(chunkId, chunkCoord, p, N + du);
    uint s2 = voxel_vis(chunkId, chunkCoord, p, N + dv);
    uint c  = voxel_vis(chunkId, chunkCoord, p, N + du + dv);

    // если оба сайда заняты — угол максимально тёмный
    if (s1 == 1u && s2 == 1u) return 3u;
    return s1 + s2 + c; // 0..3
}

// ===== emit quad =====
void emit_quad(uint chunkId, ivec3 chunkCoord, ivec3 p, uint face, uint colorRGB) {
    uint maxQuads = chunk_alloc[chunkId].needI / 6u;
    uint q = atomicAdd(emit_counter[chunkId], 1u);
    if (q >= maxQuads) return;

    uint baseV = chunk_alloc[chunkId].v_startPage * u_vb_page_verts + q * 4u;
    uint baseI = chunk_alloc[chunkId].i_startPage * u_ib_page_inds + q * 6u;

    vec3 wp = vec3(chunkCoord * u_chunk_dim + p) * u_voxel_size;

    // Определяем базис плоскости грани: нормаль N и два тангенса U,V (в воксельных шагах)
    ivec3 N, U, V;

    // и 4 вершины как раньше
    vec3 v0, v1, v2, v3;

    if (face == 0u) { // +X
        N = ivec3( 1, 0, 0);
        U = ivec3( 0, 1, 0); // v1-v0
        V = ivec3( 0, 0, 1); // v3-v0
        v0 = wp + vec3(u_voxel_size.x, 0, 0);
        v1 = wp + vec3(u_voxel_size.x, u_voxel_size.y, 0);
        v2 = wp + vec3(u_voxel_size.x, u_voxel_size.y, u_voxel_size.z);
        v3 = wp + vec3(u_voxel_size.x, 0, u_voxel_size.z);

    } else if (face == 1u) { // -X
        N = ivec3(-1, 0, 0);
        U = ivec3( 0, 0, 1);
        V = ivec3( 0, 1, 0);
        v0 = wp + vec3(0, 0, 0);
        v1 = wp + vec3(0, 0, u_voxel_size.z);
        v2 = wp + vec3(0, u_voxel_size.y, u_voxel_size.z);
        v3 = wp + vec3(0, u_voxel_size.y, 0);

    } else if (face == 2u) { // +Y
        N = ivec3( 0, 1, 0);
        U = ivec3( 0, 0, 1);
        V = ivec3( 1, 0, 0);
        v0 = wp + vec3(0, u_voxel_size.y, 0);
        v1 = wp + vec3(0, u_voxel_size.y, u_voxel_size.z);
        v2 = wp + vec3(u_voxel_size.x, u_voxel_size.y, u_voxel_size.z);
        v3 = wp + vec3(u_voxel_size.x, u_voxel_size.y, 0);

    } else if (face == 3u) { // -Y
        N = ivec3( 0,-1, 0);
        U = ivec3( 1, 0, 0);
        V = ivec3( 0, 0, 1);
        v0 = wp + vec3(0, 0, 0);
        v1 = wp + vec3(u_voxel_size.x, 0, 0);
        v2 = wp + vec3(u_voxel_size.x, 0, u_voxel_size.z);
        v3 = wp + vec3(0, 0, u_voxel_size.z);

    } else if (face == 4u) { // +Z
        N = ivec3( 0, 0, 1);
        U = ivec3( 1, 0, 0);
        V = ivec3( 0, 1, 0);
        v0 = wp + vec3(0, 0, u_voxel_size.z);
        v1 = wp + vec3(u_voxel_size.x, 0, u_voxel_size.z);
        v2 = wp + vec3(u_voxel_size.x, u_voxel_size.y, u_voxel_size.z);
        v3 = wp + vec3(0, u_voxel_size.y, u_voxel_size.z);

    } else { // -Z
        N = ivec3( 0, 0,-1);
        U = ivec3( 0, 1, 0);
        V = ivec3( 1, 0, 0);
        v0 = wp + vec3(0, 0, 0);
        v1 = wp + vec3(0, u_voxel_size.y, 0);
        v2 = wp + vec3(u_voxel_size.x, u_voxel_size.y, 0);
        v3 = wp + vec3(u_voxel_size.x, 0, 0);
    }

    // --- AO для 4 углов ---
    // Вершины считаем как (du,dv) = (0/1, 0/1), а знак берём: 0 -> -1, 1 -> +1
    // v0: (0,0), v1:(1,0), v2:(1,1), v3:(0,1) — это соответствует твоим текущим раскладкам выше.

    uint oc0 = ao_corner(chunkId, chunkCoord, p, N, U, V, -1, -1);
    uint oc1 = ao_corner(chunkId, chunkCoord, p, N, U, V,  1, -1);
    uint oc2 = ao_corner(chunkId, chunkCoord, p, N, U, V,  1,  1);
    uint oc3 = ao_corner(chunkId, chunkCoord, p, N, U, V, -1,  1);

    uint c0 = pack_ao_in_alpha(colorRGB, oc0);
    uint c1 = pack_ao_in_alpha(colorRGB, oc1);
    uint c2 = pack_ao_in_alpha(colorRGB, oc2);
    uint c3 = pack_ao_in_alpha(colorRGB, oc3);

    vb[baseV + 0u].pos = vec4(v0, 1.0); vb[baseV + 0u].color = c0; vb[baseV + 0u].face = face;
    vb[baseV + 1u].pos = vec4(v1, 1.0); vb[baseV + 1u].color = c1; vb[baseV + 1u].face = face;
    vb[baseV + 2u].pos = vec4(v2, 1.0); vb[baseV + 2u].color = c2; vb[baseV + 2u].face = face;
    vb[baseV + 3u].pos = vec4(v3, 1.0); vb[baseV + 3u].color = c3; vb[baseV + 3u].face = face;


    ib[baseI + 0u] = baseV + 0u;
    ib[baseI + 1u] = baseV + 1u;
    ib[baseI + 2u] = baseV + 2u;
    ib[baseI + 3u] = baseV + 0u;
    ib[baseI + 4u] = baseV + 2u;
    ib[baseI + 5u] = baseV + 3u;

    // --- диагональ по AO (убирает "шахматные" швы) ---
    // сравниваем суммы окклюзий по диагоналям: меньше окклюзия -> светлее -> лучше выбрать соответствующую диагональ

    uint s02 = oc0 + oc2;
    uint s13 = oc1 + oc3;

    if (s02 <= s13) {
        // диагональ 0-2
        ib[baseI + 0u] = baseV + 0u;
        ib[baseI + 1u] = baseV + 1u;
        ib[baseI + 2u] = baseV + 2u;
        ib[baseI + 3u] = baseV + 0u;
        ib[baseI + 4u] = baseV + 2u;
        ib[baseI + 5u] = baseV + 3u;
    } else {
        // диагональ 1-3
        ib[baseI + 0u] = baseV + 0u;
        ib[baseI + 1u] = baseV + 1u;
        ib[baseI + 2u] = baseV + 3u;
        ib[baseI + 3u] = baseV + 1u;
        ib[baseI + 4u] = baseV + 2u;
        ib[baseI + 5u] = baseV + 3u;
    }
}

void main() {
    if (is_vb_full == 1u || is_ib_full == 1u)
        return;

    uint voxelId  = gl_GlobalInvocationID.x;
    uint dirtyIdx = gl_GlobalInvocationID.y;

    uint dirtyCount = dirty_count;
    if (dirtyIdx >= dirtyCount) return;
    if (voxelId >= u_voxels_per_chunk) return;

    uint chunkId = dirty_list[dirtyIdx];
    if (chunk_alloc[chunkId].v_startPage == INVALID_ID || chunk_alloc[chunkId].i_startPage == INVALID_ID) return;

    uvec2 key2 = uvec2(meta[chunkId].key_lo, meta[chunkId].key_hi);
    ivec3 chunkCoord = unpack_key_to_coord(key2, u_pack_offset, u_pack_bits);

    int lx = int(voxelId % uint(u_chunk_dim.x));
    int ly = int((voxelId / uint(u_chunk_dim.x)) % uint(u_chunk_dim.y));
    int lz = int(voxelId / uint(u_chunk_dim.x * u_chunk_dim.y));
    ivec3 p = ivec3(lx, ly, lz);

    VoxelData voxel_data = voxel_data_in_chunk(chunkId, p);
    if ((read_voxel_flags(voxel_data.type_flags) & VOXEL_VISABILITY_FLAG_BIT) == 0u) return;

    uint color = voxel_data.color;

    if (voxel_vis(chunkId, chunkCoord, p, ivec3( 1, 0, 0)) == 0u) emit_quad(chunkId, chunkCoord, p, 0u, color);
    if (voxel_vis(chunkId, chunkCoord, p, ivec3(-1, 0, 0)) == 0u) emit_quad(chunkId, chunkCoord, p, 1u, color);
    if (voxel_vis(chunkId, chunkCoord, p, ivec3( 0, 1, 0)) == 0u) emit_quad(chunkId, chunkCoord, p, 2u, color);
    if (voxel_vis(chunkId, chunkCoord, p, ivec3( 0,-1, 0)) == 0u) emit_quad(chunkId, chunkCoord, p, 3u, color);
    if (voxel_vis(chunkId, chunkCoord, p, ivec3( 0, 0, 1)) == 0u) emit_quad(chunkId, chunkCoord, p, 4u, color);
    if (voxel_vis(chunkId, chunkCoord, p, ivec3( 0, 0,-1)) == 0u) emit_quad(chunkId, chunkCoord, p, 5u, color);
}
