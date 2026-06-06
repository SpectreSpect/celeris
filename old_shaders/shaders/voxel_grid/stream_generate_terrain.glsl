#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) buffer ChunkHashTable { HashTableCounters chunk_hash_table_counters; ChunkHashTableSlot chunk_hash_table_slots[]; };
layout(std430, binding=1) readonly buffer LoadList { uint load_list_counter; uint load_list[]; };
layout(std430, binding=2) buffer ChunkVoxels { VoxelData voxels[]; };
layout(std430, binding=3) buffer ChunkMetaBuf { ChunkMeta meta[]; };
layout(std430, binding=4) buffer EnqueuedBuf { uint enqueued[]; };
layout(std430, binding=5) buffer DirtyListBuf { uint dirty_count; uint dirty_list[]; };

uniform ivec3 u_chunk_dim;
uniform uint  u_voxels_per_chunk;

uniform uint u_pack_bits;
uniform int  u_pack_offset;

uniform uint u_seed;

uniform uint u_chunk_hash_table_size;

// ----- include -----
#include "../utils.glsl"
#include "chunk_pool/mark_dirty.glsl"
// -------------------

// ---- noise (value noise + fbm) ----
float valueNoise(vec2 x) {
    ivec2 i = ivec2(floor(x));
    vec2  f = fract(x);
    vec2  u = f*f*(3.0 - 2.0*f);

    float a = hash_ivec2(i + ivec2(0,0), u_seed);
    float b = hash_ivec2(i + ivec2(1,0), u_seed);
    float c = hash_ivec2(i + ivec2(0,1), u_seed);
    float d = hash_ivec2(i + ivec2(1,1), u_seed);

    return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
}

float fbm(vec2 p) {
    float s = 0.0;
    float a = 0.5;
    for (int o=0; o<5; ++o) {
        s += a * valueNoise(p);
        p *= 2.0;
        a *= 0.5;
    }
    return s;
}

void main() {
    uint voxelId = gl_GlobalInvocationID.x;
    uint listIdx = gl_GlobalInvocationID.y;

    uint loadCount = load_list_counter;
    if (listIdx >= loadCount) return;
    if (voxelId >= u_voxels_per_chunk) return;

    uint chunkId = load_list[listIdx];

    ivec3 chunkCoord = unpack_key_to_coord(uvec2(meta[chunkId].key_lo, meta[chunkId].key_hi), u_pack_offset, u_pack_bits);

    int lx = int(voxelId % uint(u_chunk_dim.x));
    int ly = int((voxelId / uint(u_chunk_dim.x)) % uint(u_chunk_dim.y));
    int lz = int(voxelId / uint(u_chunk_dim.x * u_chunk_dim.y));
    ivec3 local = ivec3(lx, ly, lz);

    ivec3 worldVoxel = chunkCoord * u_chunk_dim + local;

    // // ---- terrain height from fbm(xz) ----
    vec2 xz = vec2(worldVoxel.x, worldVoxel.z) * 0.03; // частота
    float n = fbm(xz); // 0..~1
    // float n = 0.5f;
    float height = 20.0 + n * 30.0; // базовый уровень + амплитуда

    uint type = (float(worldVoxel.y) <= height) ? 1u : 0u;
    uint voxel_flags = (type != 0u) ? VOXEL_VISABILITY_FLAG_BIT : VOXEL_EASY_OVERWRITE_FLAG_BIT;

    VoxelData vd;
    vd.type_flags = pack_voxel_type_flags(type, voxel_flags);
    // цвет: чуть меняем по высоте
    vec3 col = (type != 0u) ? mix(vec3(0.15,0.35,0.10), vec3(0.45,0.30,0.15), n) : vec3(0.0);
    vd.color = pack_color(col);

    uint base = chunkId * u_voxels_per_chunk;

    uint global_voxel_id = base + voxelId;
    voxels[global_voxel_id] = vd;

    // один раз на чанк
    if (voxelId == 0u) {
        mark_dirty_around(chunkId, chunkCoord);
    }
}
