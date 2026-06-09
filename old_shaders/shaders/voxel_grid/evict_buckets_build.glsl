#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) readonly buffer ChunkMetaBuf { ChunkMeta meta[]; };
layout(std430, binding=1) coherent buffer BucketHeads { BucketHead bucket_heads[]; };
layout(std430, binding=2) coherent buffer BucketNext  { uint bucket_next[]; };

uniform uint  u_max_chunks;
uniform uint  u_bucket_count;

uniform vec3  u_cam_pos;
uniform ivec3 u_chunk_dim;
uniform vec3  u_voxel_size;

uniform uint u_pack_bits;
uniform int  u_pack_offset;

uniform float f_eviction_bucket_shell_thickness;

// ----- include -----
#include "../utils.glsl"
// -------------------

void push_bucket(uint b, uint chunkId) {
    uint old = atomicExchange(bucket_heads[b].id, chunkId);
    bucket_next[chunkId] = old;
    atomicAdd(bucket_heads[b].count, 1u);

    // for (;;) {
    //     uint old = atomicAdd(bucket_heads[b].id, 0u);
    //     bucket_next[chunkId] = old;
    //     memoryBarrierBuffer(); // next должен стать видим до публикации head
    //     uint prev = atomicCompSwap(bucket_heads[b].id, old, chunkId);
    //     if (prev == old) {
    //         atomicAdd(bucket_heads[b].count, 1u);
    //         break;
    //     }
    // }
}

uint bucket_for_coord(ivec3 chunkCoord) {
    vec3 chunkSize = vec3(u_chunk_dim) * u_voxel_size;
    vec3 minP = vec3(chunkCoord * u_chunk_dim) * u_voxel_size;
    vec3 center = minP + 0.5 * chunkSize;

    vec3 d = center - u_cam_pos;
    float dist = sqrt(dot(d, d));

    uint b = uint(dist / f_eviction_bucket_shell_thickness);
    if (b >= u_bucket_count) b = u_bucket_count - 1u;
    return b;
}

void main() {
    uint chunkId = gl_GlobalInvocationID.x;
    if (chunkId >= u_max_chunks) return;
    if (meta[chunkId].used == 0u) return;

    uvec2 key2 = uvec2(meta[chunkId].key_lo, meta[chunkId].key_hi);
    ivec3 cc = unpack_key_to_coord(key2, u_pack_offset, u_pack_bits);

    uint b = bucket_for_coord(cc);
    push_bucket(b, chunkId);
}
