#ifndef A_STAR_PATH_PASSABILITY
#define A_STAR_PATH_PASSABILITY

const float PATH_PASSABILITY_INF = 3.402823466e38;

int abs_i(int v) {
    return v < 0 ? -v : v;
}

int sign_i(int v) {
    return v < 0 ? -1 : (v > 0 ? 1 : 0);
}

struct VoxelLookupCache {
    bool valid;
    bool found;
    ivec3 chunk_coord;
    uint chunk_id;
};

void init_voxel_lookup_cache(out VoxelLookupCache cache) {
    cache.valid = false;
    cache.found = false;
    cache.chunk_coord = ivec3(0);
    cache.chunk_id = INVALID_ID;
}

bool cached_chunk_contains(VoxelLookupCache cache, ivec3 pos, out ivec3 local_pos) {
    if (!cache.valid) return false;

    local_pos = pos - cache.chunk_coord * u_chunk_dim.xyz;
    return all(greaterThanEqual(local_pos, ivec3(0))) &&
           all(lessThan(local_pos, u_chunk_dim.xyz));
}

uint voxel_flags(ivec3 pos, inout VoxelLookupCache cache) {
    ivec3 local_pos;
    if (cached_chunk_contains(cache, pos, local_pos)) {
        if (!cache.found) return 0u;

        VoxelData voxel_data = voxel_data_in_chunk(cache.chunk_id, local_pos);
        return read_voxel_flags(voxel_data.type_flags);
    }

    ivec3 chunk_pos = floor_div(pos, u_chunk_dim.xyz);
    local_pos = pos - chunk_pos * u_chunk_dim.xyz;

    cache.valid = true;
    cache.found = false;
    cache.chunk_coord = chunk_pos;
    cache.chunk_id = INVALID_ID;

    uvec2 key = pack_key_uvec2(chunk_pos, u_pack_offset, u_pack_bits);
    uint slot_id = chunk_hash_table_lookup_hash_table_slot_id(key, true);
    if (slot_id == INVALID_ID) return 0u;

    cache.found = true;
    cache.chunk_id = chunk_hash_table_slots[slot_id].value;

    VoxelData voxel_data = voxel_data_in_chunk(cache.chunk_id, local_pos);
    return read_voxel_flags(voxel_data.type_flags);
}

bool is_blocking_voxel(ivec3 pos, inout VoxelLookupCache cache) {
    uint flags = voxel_flags(pos, cache);
    return (flags & (VOXEL_VISABILITY_FLAG_BIT | VOXEL_INFLATED_BIT)) != 0u;
}

bool is_ground_voxel(ivec3 pos, inout VoxelLookupCache cache) {
    // return (voxel_flags(pos, cache) & VOXEL_VISABILITY_FLAG_BIT) != 0u;
    uint flags = voxel_flags(pos, cache);
    return (flags & (VOXEL_VISABILITY_FLAG_BIT | VOXEL_INFLATED_BIT)) != 0u;
}

int voxel_y_offset(ivec3 pos, uint max_step_up_u, uint max_drop_u, inout VoxelLookupCache cache) {
    int max_step_up = int(max_step_up_u);
    int max_drop = int(max_drop_u);

    if (is_blocking_voxel(pos, cache)) {
        for (int dy = 1; dy <= max_step_up; dy++) {
            ivec3 stand_pos = pos + ivec3(0, dy, 0);

            if (!is_blocking_voxel(stand_pos, cache)) {
                return dy;
            }
        }

        return max_step_up + 1;
    }

    for (int drop = 0; drop <= max_drop; drop++) {
        ivec3 stand_pos = pos + ivec3(0, -drop, 0);
        ivec3 ground_pos = stand_pos + ivec3(0, -1, 0);

        if (!is_blocking_voxel(stand_pos, cache) && is_ground_voxel(ground_pos, cache)) {
            return -drop;
        }
    }

    if (u_allow_flying_over_precipices != 0u) {
        return 0;
    }

    return -(max_drop + 1);
}

int sign_f(float v) {
    return v < 0.0 ? -1 : (v > 0.0 ? 1 : 0);
}

float init_t_max(float start, float dir, int voxel, int step_dir) {
    if (step_dir == 0) return PATH_PASSABILITY_INF;

    float next_boundary = step_dir > 0 ? float(voxel + 1) : float(voxel);
    return (next_boundary - start) / dir;
}

bool is_voxel_segment_unblocked(vec3 from_voxel_point, vec3 to_voxel_point) {
    VoxelLookupCache cache;
    init_voxel_lookup_cache(cache);

    ivec3 current_pos = ivec3(floor(from_voxel_point));
    ivec3 to_pos = ivec3(floor(to_voxel_point));

    if (is_blocking_voxel(current_pos, cache)) {
        return false;
    }

    if (all(equal(current_pos, to_pos))) {
        return true;
    }

    vec3 dir = to_voxel_point - from_voxel_point;
    ivec3 step_dir = ivec3(sign_f(dir.x), sign_f(dir.y), sign_f(dir.z));

    vec3 t_delta = vec3(
        step_dir.x == 0 ? PATH_PASSABILITY_INF : 1.0 / abs(dir.x),
        step_dir.y == 0 ? PATH_PASSABILITY_INF : 1.0 / abs(dir.y),
        step_dir.z == 0 ? PATH_PASSABILITY_INF : 1.0 / abs(dir.z)
    );

    vec3 t_max = vec3(
        init_t_max(from_voxel_point.x, dir.x, current_pos.x, step_dir.x),
        init_t_max(from_voxel_point.y, dir.y, current_pos.y, step_dir.y),
        init_t_max(from_voxel_point.z, dir.z, current_pos.z, step_dir.z)
    );

    ivec3 voxel_diff = ivec3(
        abs_i(to_pos.x - current_pos.x),
        abs_i(to_pos.y - current_pos.y),
        abs_i(to_pos.z - current_pos.z)
    );

    int max_steps = voxel_diff.x + voxel_diff.y + voxel_diff.z;

    for (int step = 0; step < max_steps; step++) {
        if (t_max.x <= t_max.y && t_max.x <= t_max.z) {
            current_pos.x += step_dir.x;
            t_max.x += t_delta.x;
        } else if (t_max.y <= t_max.z) {
            current_pos.y += step_dir.y;
            t_max.y += t_delta.y;
        } else {
            current_pos.z += step_dir.z;
            t_max.z += t_delta.z;
        }

        if (is_blocking_voxel(current_pos, cache)) {
            return false;
        }

        if (all(equal(current_pos, to_pos))) {
            return true;
        }
    }

    return !is_blocking_voxel(to_pos, cache);
}

bool try_make_step(ivec3 pos, ivec2 step_dir_xz, out ivec3 next, inout VoxelLookupCache cache) {
    next = pos + ivec3(step_dir_xz.x, 0, step_dir_xz.y);

    int y_offset = voxel_y_offset(next, u_max_step_up, u_max_drop, cache);

    if (y_offset >= -int(u_max_drop) && y_offset <= int(u_max_step_up)) {
        next.y += y_offset;
        return true;
    }

    next = pos;
    return false;
}

bool make_step(inout ivec3 current_pos, ivec2 step_dir_xz, inout VoxelLookupCache cache) {
    ivec3 next;

    if (!try_make_step(current_pos, step_dir_xz, next, cache)) {
        return false;
    }

    current_pos = next;
    return true;
}

bool make_diagonal_step(inout ivec3 current_pos, ivec2 step_dir_xz, inout VoxelLookupCache cache) {
    if (u_allow_diagonal_moves == 0u) {
        return false;
    }

    ivec3 next;
    if (!try_make_step(current_pos, step_dir_xz, next, cache)) {
        return false;
    }

    current_pos = next;
    return true;
}

bool is_path_unimpended(ivec3 from_pos, ivec3 to_pos) {
    VoxelLookupCache cache;
    init_voxel_lookup_cache(cache);

    if (is_blocking_voxel(from_pos, cache))
        return false;
    
    ivec3 diff = to_pos - from_pos;

    ivec3 current_pos = from_pos;

    ivec3 sign_dir = ivec3(sign_i(diff.x), sign_i(diff.y), sign_i(diff.z));
    ivec3 abs_diff = ivec3(abs_i(diff.x), abs_i(diff.y), abs_i(diff.z));

    ivec3 abs_offset = ivec3(0, 0, 0);
    
    int max_steps = abs_diff.x + abs_diff.z;
    int max_possible_step_up = max_steps * int(u_max_step_up);
    int max_possible_drop = max_steps * int(u_max_drop);

    if (diff.y > max_possible_step_up || -diff.y > max_possible_drop)
        return false;

    while (abs_offset.x < abs_diff.x || abs_offset.z < abs_diff.z) {
        if (abs_diff.x == 0) {
            if (!make_step(current_pos, ivec2(0, sign_dir.z), cache))
                return false;

            abs_offset.z++;
        } else if (abs_diff.z == 0) {
            if (!make_step(current_pos, ivec2(sign_dir.x, 0), cache))
                return false;

            abs_offset.x++;
        } else {
            int lhs = (2 * abs_offset.x + 1) * abs_diff.z;
            int rhs = (2 * abs_offset.z + 1) * abs_diff.x;

            if (lhs < rhs) {
                if (!make_step(current_pos, ivec2(sign_dir.x, 0), cache))
                    return false;

                abs_offset.x++;
            } else if (lhs > rhs) {
                if (!make_step(current_pos, ivec2(0, sign_dir.z), cache))
                    return false;

                abs_offset.z++;
            } else {
                ivec3 next;

                if (make_diagonal_step(current_pos, ivec2(sign_dir.x, sign_dir.z), cache)) {
                    abs_offset += ivec3(1, 0, 1);
                    continue;
                }

                if (try_make_step(current_pos, ivec2(sign_dir.x, 0), next, cache) && 
                    make_step(next, ivec2(0, sign_dir.z), cache))
                {
                    current_pos = next;
                    abs_offset += ivec3(1, 0, 1);
                    continue;
                }

                if (try_make_step(current_pos, ivec2(0, sign_dir.z), next, cache) && 
                    make_step(next, ivec2(sign_dir.x, 0), cache))
                {
                    current_pos = next;
                    abs_offset += ivec3(1, 0, 1);
                    continue;
                }

                return false;
            }
        }
    }

    return current_pos.y == to_pos.y;
}

#endif
