#pragma once

vec4 voxel_index_to_position(uint voxel_id) {
    uint position_offset_bytes = voxel_id * u_vertex_stride_bytes + u_vertex_position_offset_bytes;
    return vbo_data[position_offset_bytes / 16u];
}

void load_triangle_positions(uint triangle_id, out vec4 triangle_positions[3]) {
    uint vi0 = ebo_data[triangle_id*3u + 0u];
    uint vi1 = ebo_data[triangle_id*3u + 1u];
    uint vi2 = ebo_data[triangle_id*3u + 2u];

    triangle_positions[0u] = voxel_index_to_position(vi0);
    triangle_positions[1u] = voxel_index_to_position(vi1);
    triangle_positions[2u] = voxel_index_to_position(vi2);
}

