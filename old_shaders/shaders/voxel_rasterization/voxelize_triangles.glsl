#version 430
layout(local_size_x = 256) in;

// ----- include -----
#include "../common/buffer_structures.glsl"
// -------------------

layout(std430, binding=0) buffer CounterHashTable { HashTableCounters counter_hash_table_counters; CounterHashTableSlot counter_hash_table_slots[]; };
layout(std430, binding=1) buffer ActiveChunkKeysList { uint active_chunk_keys_counter; uvec2 active_chunk_keys_list[]; };
layout(std430, binding=2) buffer TriangleIndicesList { uint triangle_counter; uint triangle_indices_list[]; };
layout(std430, binding=3) buffer VBO { float vbo_data[]; };
layout(std430, binding=4) buffer EBO { uint ebo_data[]; };
layout(std430, binding=5) buffer VoxelsWriteData { uint count_voxel_writes; uint pad_[3u]; VoxelWrite voxel_writes[]; };


uniform uint u_counter_hash_table_size;
uniform uint u_count_voxels_in_chunk;

uniform uint u_vertex_stride_bytes;
uniform uint u_vertex_position_offset_bytes;

uniform uvec3 u_chunk_dim;
uniform vec3 u_voxel_size;

uniform mat4 u_transform;

uniform uint u_pack_offset;
uniform uint u_pack_bits;

uniform uint voxel_type_flags;
uniform uint voxel_color;
uniform uint voxel_set_flags;

// ----- include -----
#include "../utils.glsl"
#include "counter_hash_table/lookup_remove.glsl"
// -------------------


bool axis_overlap(vec3 axis, vec3 v0, vec3 v1, vec3 v2, vec3 half_size)
{
    // Если ось почти нулевая (дег. ребро/треугольник), тест пропускаем
    float len2 = dot(axis, axis);
    if (len2 < 1e-12) return true;

    float p0 = dot(v0, axis);
    float p1 = dot(v1, axis);
    float p2 = dot(v2, axis);

    float mn = min(p0, min(p1, p2));
    float mx = max(p0, max(p1, p2));

    float r = dot(abs(axis), half_size);
    return !(mn > r || mx < -r);
}

bool plane_box_overlap(vec3 normal, vec3 v0, vec3 half_size)
{
    // Проверка пересечения плоскости треугольника с AABB
    vec3 vmin, vmax;

    // Для каждой координаты выбираем экстремальные точки бокса относительно normal
    vmin.x = (normal.x > 0.0) ? -half_size.x - v0.x :  half_size.x - v0.x;
    vmax.x = (normal.x > 0.0) ?  half_size.x - v0.x : -half_size.x - v0.x;

    vmin.y = (normal.y > 0.0) ? -half_size.y - v0.y :  half_size.y - v0.y;
    vmax.y = (normal.y > 0.0) ?  half_size.y - v0.y : -half_size.y - v0.y;

    vmin.z = (normal.z > 0.0) ? -half_size.z - v0.z :  half_size.z - v0.z;
    vmax.z = (normal.z > 0.0) ?  half_size.z - v0.z : -half_size.z - v0.z;

    if (dot(normal, vmin) > 0.0) return false;
    return dot(normal, vmax) >= 0.0;
}

bool tri_box_overlap(vec3 box_center, vec3 half_size, vec3 p0, vec3 p1, vec3 p2)
{
    // Переводим в координаты бокса (центр бокса в 0)
    vec3 v0 = p0 - box_center;
    vec3 v1 = p1 - box_center;
    vec3 v2 = p2 - box_center;

    // 1) AABB тест: tri AABB vs box
    vec3 mn = min(v0, min(v1, v2));
    vec3 mx = max(v0, max(v1, v2));
    if (mn.x >  half_size.x || mx.x < -half_size.x) return false;
    if (mn.y >  half_size.y || mx.y < -half_size.y) return false;
    if (mn.z >  half_size.z || mx.z < -half_size.z) return false;

    // Рёбра треугольника
    vec3 e0 = v1 - v0;
    vec3 e1 = v2 - v1;
    vec3 e2 = v0 - v2;

    // 2) 9 SAT тестов: (edge x axisX/Y/Z)
    // axis = edge x X => (0, edge.z, -edge.y) (знак неважен)
    if (!axis_overlap(vec3(0.0,  e0.z, -e0.y), v0, v1, v2, half_size)) return false;
    if (!axis_overlap(vec3(0.0,  e1.z, -e1.y), v0, v1, v2, half_size)) return false;
    if (!axis_overlap(vec3(0.0,  e2.z, -e2.y), v0, v1, v2, half_size)) return false;

    // axis = edge x Y => (-edge.z, 0, edge.x)
    if (!axis_overlap(vec3(-e0.z, 0.0,  e0.x), v0, v1, v2, half_size)) return false;
    if (!axis_overlap(vec3(-e1.z, 0.0,  e1.x), v0, v1, v2, half_size)) return false;
    if (!axis_overlap(vec3(-e2.z, 0.0,  e2.x), v0, v1, v2, half_size)) return false;

    // axis = edge x Z => (edge.y, -edge.x, 0)
    if (!axis_overlap(vec3( e0.y, -e0.x, 0.0), v0, v1, v2, half_size)) return false;
    if (!axis_overlap(vec3( e1.y, -e1.x, 0.0), v0, v1, v2, half_size)) return false;
    if (!axis_overlap(vec3( e2.y, -e2.x, 0.0), v0, v1, v2, half_size)) return false;

    // 3) plane-box
    vec3 normal = cross(e0, v2 - v0);
    if (!plane_box_overlap(normal, v0, half_size)) return false;

    return true;
}

vec4 voxel_index_to_position(uint voxel_id) {
    uint position_offset_bytes = voxel_id * u_vertex_stride_bytes + u_vertex_position_offset_bytes;
    uint position_offset_index = position_offset_bytes / 4u;
    float x = vbo_data[position_offset_index + 0u];
    float y = vbo_data[position_offset_index + 1u];
    float z = vbo_data[position_offset_index + 2u];
    return vec4(x, y, z, 1.0f);
}

void main() {
    uint voxel_id = gl_GlobalInvocationID.x;
    uint active_key_list_id = gl_GlobalInvocationID.y;
    if (voxel_id >= u_count_voxels_in_chunk) return;
    if (active_key_list_id >= active_chunk_keys_counter) return;

    uvec2 chunk_key = active_chunk_keys_list[active_key_list_id];
    uint slot_id = counter_hash_table_lookup_hash_table_slot_id(chunk_key, true);
    if (slot_id == INVALID_ID) return;

    ivec3 chunk_pos = unpack_key_to_coord(chunk_key, u_pack_offset, u_pack_bits);

    uint voxel_x = voxel_id % u_chunk_dim.x;
    uint voxel_yz = voxel_id / u_chunk_dim.x;
    uint voxel_y = voxel_yz % u_chunk_dim.y;
    uint voxel_z = voxel_yz / u_chunk_dim.y;

    ivec3 local_voxel_pos = ivec3(voxel_x, voxel_y, voxel_z);
    ivec3 global_voxel_pos = chunk_pos * ivec3(u_chunk_dim) + local_voxel_pos;

    vec3 half_voxel_size = u_voxel_size * 0.5;
    vec3 render_voxel_center = vec3(global_voxel_pos) * u_voxel_size + half_voxel_size;
    
    CounterAllocMeta meta = counter_hash_table_slots[slot_id].value;

    for (uint triangle_list_id = 0u; triangle_list_id < meta.count_triangles; triangle_list_id++) {
        uint triangle_id = triangle_indices_list[meta.triangle_indices_base + triangle_list_id];

        uint vi0 = ebo_data[triangle_id*3u + 0u];
        uint vi1 = ebo_data[triangle_id*3u + 1u];
        uint vi2 = ebo_data[triangle_id*3u + 2u];

        vec3 p0 = (u_transform * voxel_index_to_position(vi0)).xyz;
        vec3 p1 = (u_transform * voxel_index_to_position(vi1)).xyz;
        vec3 p2 = (u_transform * voxel_index_to_position(vi2)).xyz;
        
        // vec3 diff = p0 - render_voxel_center;
        // if (dot(diff, diff) > 10) continue;

        if (tri_box_overlap(render_voxel_center, half_voxel_size, p0, p1, p2)) {
            VoxelWrite voxel_write;
            voxel_write.world_voxel = ivec4(global_voxel_pos.xyz, 0);
            voxel_write.voxel_data.type_flags = voxel_type_flags;
            voxel_write.voxel_data.color = voxel_color;
            voxel_write.set_flags = voxel_set_flags;

            uint voxel_write_id = atomicAdd(count_voxel_writes, 1u);
            voxel_writes[voxel_write_id] = voxel_write;
            return;
        }
    }
}
