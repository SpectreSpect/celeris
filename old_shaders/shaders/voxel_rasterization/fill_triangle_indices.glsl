#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) buffer CounterHashTable { HashTableCounters counter_hash_table_counters; CounterHashTableSlot counter_hash_table_slots[]; };
layout(std430, binding=1) buffer TriangleIndicesList { uint triangle_counter; uint triangle_indices_list[]; };
layout(std430, binding=2) buffer VBO { float vbo_data[]; };
layout(std430, binding=3) buffer EBO { uint ebo_data[]; };

uniform uint u_count_mesh_triangles;
uniform uint u_counter_hash_table_size;

uniform uint u_vertex_stride_bytes;
uniform uint u_vertex_position_offset_bytes;

uniform vec3 u_voxel_size;
uniform uvec3 u_chunk_size;

uniform uint u_pack_offset;
uniform uint u_pack_bits;

uniform mat4 u_transform;

// ----- include -----
#include "../utils.glsl"
#include "counter_hash_table/lookup_remove.glsl"
// -------------------

vec4 voxel_index_to_position(uint voxel_id) {
    uint position_offset_bytes = voxel_id * u_vertex_stride_bytes + u_vertex_position_offset_bytes;
    uint position_offset_index = position_offset_bytes / 4u;
    float x = vbo_data[position_offset_index + 0u];
    float y = vbo_data[position_offset_index + 1u];
    float z = vbo_data[position_offset_index + 2u];
    return vec4(x, y, z, 1.0f);
}

void main() {
    uint triangle_id = gl_GlobalInvocationID.x;
    if (triangle_id >= u_count_mesh_triangles) return;

    uint vi0 = ebo_data[triangle_id*3u + 0u];
    uint vi1 = ebo_data[triangle_id*3u + 1u];
    uint vi2 = ebo_data[triangle_id*3u + 2u];

    vec4 p0_local = voxel_index_to_position(vi0);
    vec4 p1_local = voxel_index_to_position(vi1);
    vec4 p2_local = voxel_index_to_position(vi2);

    vec3 render_chunk_size = u_voxel_size * u_chunk_size;
    ivec3 p0 = ivec3(floor((u_transform * p0_local).xyz / render_chunk_size));
    ivec3 p1 = ivec3(floor((u_transform * p1_local).xyz / render_chunk_size));
    ivec3 p2 = ivec3(floor((u_transform * p2_local).xyz / render_chunk_size));

    ivec3 min_p = min(p0, min(p1, p2));
    ivec3 max_p = max(p0, max(p1, p2));

    for (int x = min_p.x; x <= max_p.x; x++)
    for (int y = min_p.y; y <= max_p.y; y++)
    for (int z = min_p.z; z <= max_p.z; z++) {
        uvec2 key = pack_key_uvec2(ivec3(x, y, z), u_pack_offset, u_pack_bits);
        
        uint slot_id = counter_hash_table_lookup_hash_table_slot_id(key, true);
        if (slot_id == INVALID_ID) continue;

        uint base = atomicAdd(counter_hash_table_slots[slot_id].value.triangle_emmit_counter, 1u);
        triangle_indices_list[base] = triangle_id;
    }
}
