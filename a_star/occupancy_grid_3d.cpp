#include "occupancy_grid_3d.h"

#include "../voxel_grid_vulkan/shader_helper/buffer_dispatch_arg.h"
#include "../vulkan_self/push_constants_structures.h"
#include "../managers/compute_pass_manager.h"
#include "../voxel_grid_vulkan/voxel_grid.h"
#include "../math_utils.h"

OccupancyGrid3D::OccupancyGrid3D(
    VulkanPhysicalDevice& physical_device, 
    VulkanDevice& device, 
    VoxelGrid& voxel_grid, 
    ComputePassManager& compute_pass_manager
) 
    :   m_voxel_grid(&voxel_grid),
        m_prepare_copy_dirty_list_dispatch_args_pi(
            compute_pass_manager.prepare_copy_dirty_list_dispatch_args_cp,
            compute_pass_manager.descriptor_pool()),
        m_copy_dirty_list_pi(compute_pass_manager.copy_dirty_list_cp, compute_pass_manager.descriptor_pool()),
        // voxel_grid.buffers().dirty_list.size()
        m_dirty_chunk_position_buffer(VulkanBuffer::create_host_visible_storage_buffer(
            physical_device, 
            device, 
            (voxel_grid.params().count_active_chunks + 1) * sizeof(glm::ivec4)
        )
        ) {
    voxel_grid.add_next_to_stream_chunks_sphere_callback([&](VulkanCommandBuffer& command_buffer, VoxelGrid& voxel_grid) {

        m_prepare_copy_dirty_list_dispatch_args_pi.set_storage_buffer(0, voxel_grid.buffers().dirty_list);
        m_prepare_copy_dirty_list_dispatch_args_pi.set_storage_buffer(1, voxel_grid.buffers().dispatch_args);

        m_prepare_copy_dirty_list_dispatch_args_pi.bind(command_buffer);

        command_buffer.dispatch(1, 1, 1);

        voxel_grid.buffers().dispatch_args.memory_barrier(
            command_buffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT
        );


        // voxel_grid.shader_helper().prepare_dispatch_args(
        //     command_buffer, 
        //     voxel_grid.buffers().dispatch_args, 
        //     BufferDispatchArg(&voxel_grid.buffers().dirty_list, 0u));
        
        m_copy_dirty_list_pi.set_storage_buffer(0, voxel_grid.buffers().dirty_list);
        m_copy_dirty_list_pi.set_storage_buffer(1, m_dirty_chunk_position_buffer);
        m_copy_dirty_list_pi.set_storage_buffer(2, voxel_grid.buffers().chunk_meta);

        m_copy_dirty_list_pi.push_constants(command_buffer, CopyDirtyListPushConstants{
            .u_pack_bits = math_utils::BITS,
            .u_pack_offset = math_utils::OFFSET
        });

        m_copy_dirty_list_pi.bind(command_buffer);

        command_buffer.dispatch_indirect(voxel_grid.buffers().dispatch_args);

        m_dirty_chunk_position_buffer.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    });

    voxel_grid.add_next_to_update_submit_callbacks([&](VoxelGrid& voxel_grid) {
        uint32_t dirty_chunk_position_count = 0;

        m_dirty_chunk_position_buffer.read(&dirty_chunk_position_count, sizeof(uint32_t), 0);

        if (m_dirty_chunk_positions.size() < dirty_chunk_position_count)
            m_dirty_chunk_positions.resize(dirty_chunk_position_count);

        m_dirty_chunk_position_buffer.read(
            m_dirty_chunk_positions.data(), 
            dirty_chunk_position_count * sizeof(uint32_t), 
            sizeof(uint32_t)
        );

        std::lock_guard lock(m_chunk_cache_mutex);
        for (int i = 0; i < dirty_chunk_position_count; i++) {
            glm::ivec4 center_chunk_pos = m_dirty_chunk_positions[i];
            uint64_t center_chunk_key = math_utils::pack_key(
                center_chunk_pos.x, 
                center_chunk_pos.y, 
                center_chunk_pos.z
            );

            m_chunk_cache.erase(center_chunk_key);

            for (int dx = -1; dx <= 1; dx++)
                for (int dz = -1; dz <= 1; dz++) {
                    if (dx == 0 && dz == 0)
                        continue;

                    glm::ivec4 chunk_pos = center_chunk_pos + glm::ivec4(dx, 0, dz, 0);

                    uint64_t chunk_key = math_utils::pack_key(
                        chunk_pos.x, 
                        chunk_pos.y, 
                        chunk_pos.z
                    );

                    m_chunk_cache.erase(chunk_key);
                }
        }
    });
}

void OccupancyGrid3D::clear_cache() {
    {
        std::lock_guard lock(m_chunk_cache_mutex);
        m_chunk_cache.clear();
    }

    {
        std::lock_guard lock(m_metrics_mutex);
        read_and_inflate_chunk_time = AvgTimer();
        is_solid_time = AvgTimer();
        read_and_inflate_chunk_count = 0;
        is_solid_count = 0;
    }
}

float OccupancyGrid3D::is_solid_time_ms() {
    std::lock_guard lock(m_metrics_mutex);
    return static_cast<float>(is_solid_time.total_ms());
}

uint32_t OccupancyGrid3D::solid_check_count() {
    std::lock_guard lock(m_metrics_mutex);
    return is_solid_count;
}

glm::ivec3 OccupancyGrid3D::floor_pos(const glm::vec3& p) {
    // LOG_NAMED("OccupancyGrid3D");
    return glm::ivec3((int)std::floor(p.x), (int)std::floor(p.y), (int)std::floor(p.z));
}

std::vector<glm::ivec3> OccupancyGrid3D::line_intersects(glm::vec3 pos1, glm::vec3 pos2) {
    // // LOG_NAMED("OccupancyGrid3D");

    std::vector<glm::ivec3> out;

    glm::vec3 d = pos2 - pos1;
    double len = std::sqrt((double)d.x*d.x + (double)d.y*d.y + (double)d.z*d.z);
    if (len == 0.0) {
        out.push_back(floor_pos(pos1));
        return out;
    }

    // Adjust end slightly backward so that if pos2 is exactly on a voxel boundary,
    // we don't "accidentally" include the voxel beyond the endpoint.
    glm::vec3 dn = d / (float)len;
    glm::vec3 endp = pos2 - dn * 1e-6f;

    glm::ivec3 v    = floor_pos(pos1);
    glm::ivec3 vend = floor_pos(endp);

    // Unique (keeps traversal order) using your pack_key
    std::unordered_set<uint64_t> seen;
    seen.reserve(256); // optional hint; remove if you want

    auto push_unique = [&](const glm::ivec3& a) {
        // NOTE: pack_key throws if out of range
        uint64_t k = math_utils::pack_key(a.x, a.y, a.z);
        if (seen.insert(k).second)
            out.push_back(a);
    };

    push_unique(v);
    if (v == vend) return out;

    auto sgn = [](float x) -> int { return (x > 0.f) - (x < 0.f); };

    int stepX = sgn(d.x), stepY = sgn(d.y), stepZ = sgn(d.z);

    const double INF = std::numeric_limits<double>::infinity();

    auto next_boundary = [](int cell, int step) -> double {
        // If moving +, next boundary is cell+1. If moving -, next is cell (lower face).
        return (step > 0) ? (double)(cell + 1) : (double)cell;
    };

    double tMaxX = (stepX != 0) ? (next_boundary(v.x, stepX) - (double)pos1.x) / (double)d.x : INF;
    double tMaxY = (stepY != 0) ? (next_boundary(v.y, stepY) - (double)pos1.y) / (double)d.y : INF;
    double tMaxZ = (stepZ != 0) ? (next_boundary(v.z, stepZ) - (double)pos1.z) / (double)d.z : INF;

    double tDeltaX = (stepX != 0) ? 1.0 / std::abs((double)d.x) : INF;
    double tDeltaY = (stepY != 0) ? 1.0 / std::abs((double)d.y) : INF;
    double tDeltaZ = (stepZ != 0) ? 1.0 / std::abs((double)d.z) : INF;

    // Numerical guard
    const double EPS = 1e-12;

    while (true) {
        double tNext = std::min(tMaxX, std::min(tMaxY, tMaxZ));
        if (tNext > 1.0 + EPS) break;

        bool hitX = std::abs(tMaxX - tNext) <= EPS;
        bool hitY = std::abs(tMaxY - tNext) <= EPS;
        bool hitZ = std::abs(tMaxZ - tNext) <= EPS;

        // Supercover: if we cross multiple planes at once, include the adjacent voxels
        // that share the edge/corner.
        glm::ivec3 base = v;

        if (hitX && hitY && hitZ) {
            glm::ivec3 vx   = base + glm::ivec3(stepX, 0, 0);
            glm::ivec3 vy   = base + glm::ivec3(0, stepY, 0);
            glm::ivec3 vz   = base + glm::ivec3(0, 0, stepZ);
            glm::ivec3 vxy  = base + glm::ivec3(stepX, stepY, 0);
            glm::ivec3 vxz  = base + glm::ivec3(stepX, 0, stepZ);
            glm::ivec3 vyz  = base + glm::ivec3(0, stepY, stepZ);
            glm::ivec3 vxyz = base + glm::ivec3(stepX, stepY, stepZ);

            push_unique(vx);  push_unique(vy);  push_unique(vz);
            push_unique(vxy); push_unique(vxz); push_unique(vyz);
            v = vxyz;
            push_unique(v);

            tMaxX += tDeltaX; tMaxY += tDeltaY; tMaxZ += tDeltaZ;
        } else if (hitX && hitY) {
            glm::ivec3 vx  = base + glm::ivec3(stepX, 0, 0);
            glm::ivec3 vy  = base + glm::ivec3(0, stepY, 0);
            glm::ivec3 vxy = base + glm::ivec3(stepX, stepY, 0);

            push_unique(vx); push_unique(vy);
            v = vxy;
            push_unique(v);

            tMaxX += tDeltaX; tMaxY += tDeltaY;
        } else if (hitX && hitZ) {
            glm::ivec3 vx  = base + glm::ivec3(stepX, 0, 0);
            glm::ivec3 vz  = base + glm::ivec3(0, 0, stepZ);
            glm::ivec3 vxz = base + glm::ivec3(stepX, 0, stepZ);

            push_unique(vx); push_unique(vz);
            v = vxz;
            push_unique(v);

            tMaxX += tDeltaX; tMaxZ += tDeltaZ;
        } else if (hitY && hitZ) {
            glm::ivec3 vy  = base + glm::ivec3(0, stepY, 0);
            glm::ivec3 vz  = base + glm::ivec3(0, 0, stepZ);
            glm::ivec3 vyz = base + glm::ivec3(0, stepY, stepZ);

            push_unique(vy); push_unique(vz);
            v = vyz;
            push_unique(v);

            tMaxY += tDeltaY; tMaxZ += tDeltaZ;
        } else if (hitX) {
            v.x += stepX;
            push_unique(v);
            tMaxX += tDeltaX;
        } else if (hitY) {
            v.y += stepY;
            push_unique(v);
            tMaxY += tDeltaY;
        } else { // hitZ
            v.z += stepZ;
            push_unique(v);
            tMaxZ += tDeltaZ;
        }

        if (v == vend) break;
    }

    return out;
}

glm::vec3 OccupancyGrid3D::voxel_size() const {
    logger().check(m_voxel_grid, "Voxel grid was null");
    return m_voxel_grid->voxel_size();
}

glm::ivec3 OccupancyGrid3D::world_to_voxel_pos(const glm::vec3& p) const {
    logger().check(m_voxel_grid, "Voxel grid was null");

    const glm::vec3 voxel_size = m_voxel_grid->voxel_size();
    logger().check(glm::all(glm::greaterThan(voxel_size, glm::vec3(0.0f))),
                   "Voxel size must be greater than zero");

    return floor_pos(p / voxel_size);
}

glm::vec3 OccupancyGrid3D::voxel_to_world_pos(const glm::ivec3& p) const {
    logger().check(m_voxel_grid, "Voxel grid was null");
    return glm::vec3(p) * m_voxel_grid->voxel_size();
}

glm::vec3 OccupancyGrid3D::voxel_center_world_pos(const glm::ivec3& p) const {
    logger().check(m_voxel_grid, "Voxel grid was null");
    return (glm::vec3(p) + glm::vec3(0.5f)) * m_voxel_grid->voxel_size();
}

bool OccupancyGrid3D::is_solid(glm::ivec3 pos) {
    // // LOG_METHOD();
    // return pos.y <= 0;
    
    // logger().check(m_voxel_grid, "Voxel grid was null");

    const auto is_solid_start = std::chrono::steady_clock::now();
    auto record_is_solid_time = [&] {
        const auto is_solid_end = std::chrono::steady_clock::now();
        std::lock_guard lock(m_metrics_mutex);
        is_solid_count++;
        is_solid_time.add(is_solid_end - is_solid_start);
    };

    glm::ivec3 chunk_pos = m_voxel_grid->chunk_pos_from_voxel_pos(pos);

    uint64_t chunk_key = math_utils::pack_key(chunk_pos.x, chunk_pos.y, chunk_pos.z);

    glm::ivec3 local_pos = m_voxel_grid->pos_in_chunk_from_global_voxel_pos(chunk_pos, pos);

    {
        std::lock_guard lock(m_chunk_cache_mutex);
        auto cached_chunk_it = m_chunk_cache.find(chunk_key);
        if (cached_chunk_it != m_chunk_cache.end()) {
            VoxelDataGPU voxel = cached_chunk_it->second.voxel(glm::uvec3(local_pos));
            record_is_solid_time();
            return voxel.is_solid() || voxel.is_inflated();
        }
    }

    VoxelGridChunk loaded_chunk;
    {
        const auto read_and_inflate_start = std::chrono::steady_clock::now();
        loaded_chunk = m_voxel_grid->read_chunk(chunk_pos);
        const auto read_and_inflate_end = std::chrono::steady_clock::now();

        std::lock_guard lock(m_metrics_mutex);
        read_and_inflate_chunk_count++;
        read_and_inflate_chunk_time.add(read_and_inflate_end - read_and_inflate_start);
    }

    VoxelDataGPU voxel;
    {
        std::lock_guard lock(m_chunk_cache_mutex);
        auto [cached_chunk_it, inserted] = m_chunk_cache.emplace(chunk_key, std::move(loaded_chunk));
        voxel = cached_chunk_it->second.voxel(glm::uvec3(local_pos));
    }

    record_is_solid_time();

    return voxel.is_solid() || voxel.is_inflated();
    // return pos.y <= 0;
}

bool OccupancyGrid3D::check_footprint(glm::vec3 origin, glm::vec3 offsets, uint32_t max_step_up) {
    return m_voxel_grid->check_footprint(origin, offsets, max_step_up);
}

std::vector<glm::ivec3> OccupancyGrid3D::line_intersects_xz(glm::vec3 pos1, glm::vec3 pos2) {
    // LOG_METHOD();

    std::vector<glm::ivec3> out;

    const glm::vec3 voxel_size = m_voxel_grid->voxel_size();
    logger().check(glm::all(glm::greaterThan(voxel_size, glm::vec3(0.0f))),
                   "Voxel size must be greater than zero");

    pos1 /= voxel_size;
    pos2 /= voxel_size;

    // Work in 2D (x,z), but output ivec3 with a fixed y layer.
    glm::vec2 p1(pos1.x, pos1.z);
    glm::vec2 p2(pos2.x, pos2.z);
    glm::vec2 d = p2 - p1;

    double len = std::sqrt((double)d.x*d.x + (double)d.y*d.y);
    int y_layer = (int)std::floor(pos1.y);

    if (len == 0.0) {
        glm::ivec3 v0((int)std::floor(pos1.x), y_layer, (int)std::floor(pos1.z));
        out.push_back(v0);
        return out;
    }

    // Pull end slightly back to avoid including cell beyond endpoint when exactly on boundary.
    glm::vec2 dn = d / (float)len;
    glm::vec2 endp2 = p2 - dn * 1e-6f;

    glm::ivec2 v2((int)std::floor(p1.x), (int)std::floor(p1.y));
    glm::ivec2 vend2((int)std::floor(endp2.x), (int)std::floor(endp2.y));

    // Unique (keeps traversal order) using your 3D pack_key (y fixed)
    std::unordered_set<uint64_t> seen;
    seen.reserve(128);

    auto push_unique = [&](const glm::ivec2& a2) {
        glm::ivec3 a3(a2.x, y_layer, a2.y); // note: a2.y is "z"
        uint64_t k = math_utils::pack_key(a3.x, a3.y, a3.z); // may throw if out of range
        if (seen.insert(k).second)
            out.push_back(a3);
    };

    push_unique(v2);
    if (v2 == vend2) return out;

    auto sgn = [](float x) -> int { return (x > 0.f) - (x < 0.f); };
    int stepX = sgn(d.x);
    int stepZ = sgn(d.y); // d.y is actually deltaZ

    const double INF = std::numeric_limits<double>::infinity();

    auto next_boundary = [](int cell, int step) -> double {
        return (step > 0) ? (double)(cell + 1) : (double)cell;
    };

    double tMaxX = (stepX != 0) ? (next_boundary(v2.x, stepX) - (double)p1.x) / (double)d.x : INF;
    double tMaxZ = (stepZ != 0) ? (next_boundary(v2.y, stepZ) - (double)p1.y) / (double)d.y : INF;

    double tDeltaX = (stepX != 0) ? 1.0 / std::abs((double)d.x) : INF;
    double tDeltaZ = (stepZ != 0) ? 1.0 / std::abs((double)d.y) : INF;

    const double EPS = 1e-12;

    while (true) {
        double tNext = std::min(tMaxX, tMaxZ);
        if (tNext > 1.0 + EPS) break;

        bool hitX = std::abs(tMaxX - tNext) <= EPS;
        bool hitZ = std::abs(tMaxZ - tNext) <= EPS;

        glm::ivec2 base = v2;

        if (hitX && hitZ) {
            // Corner hit => supercover
            glm::ivec2 vx  = base + glm::ivec2(stepX, 0);
            glm::ivec2 vz  = base + glm::ivec2(0, stepZ);
            glm::ivec2 vxz = base + glm::ivec2(stepX, stepZ);

            push_unique(vx);
            push_unique(vz);
            v2 = vxz;
            push_unique(v2);

            tMaxX += tDeltaX;
            tMaxZ += tDeltaZ;
        } else if (hitX) {
            v2.x += stepX;
            push_unique(v2);
            tMaxX += tDeltaX;
        } else { // hitZ
            v2.y += stepZ;
            push_unique(v2);
            tMaxZ += tDeltaZ;
        }

        if (v2 == vend2) break;
    }

    return out;
}

bool OccupancyGrid3D::adjust_to_ground(
    glm::ivec3& output,
    int max_step_up,
    int max_drop,
    int max_y_diff,
    bool allow_flying_over_precepices,
    uint32_t* status) {
    glm::ivec3 norm_pos = output;
    glm::ivec3 result_pos = norm_pos;

    const int ground_search_drop = max_drop + 1;

    if(!get_closest_visible_bottom_pos(norm_pos, result_pos, ground_search_drop)) {
        if (status)
            *status = 1;
        return allow_flying_over_precepices;
    }

    if ((int)norm_pos.y == (int)result_pos.y) {
        if (!get_closest_invisible_top_pos(norm_pos + glm::ivec3(0, 1, 0), result_pos, max_step_up)) {
            if (status)
                *status = 2;
            return false;
        }
    }
    else
        result_pos += glm::ivec3(0, 1, 0);
    
    if (max_y_diff >= 0) {
        int y_diff = result_pos.y - norm_pos.y;
        int abs_y_diff = std::abs(y_diff);
        if (abs_y_diff > max_y_diff) {
            if (allow_flying_over_precepices && y_diff < 0)
                return true;

            if (status)
                *status = 3;
            return false;
        }
    }
    
    output.y = result_pos.y;
    if (status)
        *status = 4;
    return true;
}

bool OccupancyGrid3D::adjust_to_ground(std::vector<glm::vec3>& output, int max_step_up, int max_drop, int max_y_diff, bool allow_flying_over_precepices) {
    // LOG_METHOD();
    bool ok = adjust_to_ground_range(output.data(), 
                                          output.data() + output.size(), 
                                          [](const glm::vec3& s){ return s;},
                                          [](glm::vec3& s, const glm::vec3& p){ s.y = p.y; },
                                          max_step_up, max_drop, max_y_diff, allow_flying_over_precepices);
    return ok;
}

bool OccupancyGrid3D::adjust_to_ground(std::vector<glm::ivec3>& output, int max_step_up, int max_drop, int max_y_diff, bool allow_flying_over_precepices) {
    for (glm::ivec3& p : output) {
        if (!adjust_to_ground(p, max_step_up, max_drop, max_y_diff, allow_flying_over_precepices))
            return false;
    }
    return true;
}

bool OccupancyGrid3D::adjust_to_ground(std::vector<NonholonomicPos>& output, int max_step_up, int max_drop, int max_y_diff, bool allow_flying_over_precepices) {
    // LOG_METHOD();
    bool ok = adjust_to_ground_range(output.data(), 
                                     output.data() + output.size(), 
                                     [](const NonholonomicPos& s){ return (glm::vec3)s.pos;},
                                     [](NonholonomicPos& s, const glm::vec3& p){ s.pos.y = p.y; },
                                     max_step_up, max_drop, max_y_diff, allow_flying_over_precepices);
    return ok;
}

bool OccupancyGrid3D::adjust_to_ground(glm::vec3& output, int max_step_up, int max_drop, int max_y_diff, bool allow_flying_over_precepices, uint32_t* status) {
    // LOG_METHOD();

    glm::ivec3 norm_pos = world_to_voxel_pos(output);
    uint32_t local_status = 0;
    uint32_t* result_status = status ? status : &local_status;

    if (!adjust_to_ground(norm_pos, max_step_up, max_drop, max_y_diff, allow_flying_over_precepices, result_status))
        return false;

    if (*result_status == 4)
        output.y = voxel_to_world_pos(norm_pos).y;

    return true;
}

bool OccupancyGrid3D::get_closest_invisible_top_pos(glm::ivec3 pos, glm::ivec3 &result, int scan_height) {
    // LOG_METHOD();
    
    for (int y = 0; y <= scan_height; y++) {
        glm::ivec3 cur_pos = pos + glm::ivec3(0, y, 0);
        // Voxel voxel = get_voxel(cur_pos);
        if (!is_solid(cur_pos)) {
            result = cur_pos;
            return true;
        }
    }
    return false;
}

bool OccupancyGrid3D::get_closest_visible_bottom_pos(glm::ivec3 pos, glm::ivec3 &result, int max_drop) {
    // LOG_METHOD();
    
    for (int y = 0; y <= max_drop; y++) {
        glm::ivec3 cur_pos = pos - glm::ivec3(0, y, 0);
        // Voxel voxel = get_voxel(cur_pos);
        if (is_solid(cur_pos)) {
            result = cur_pos;
            return true;
        }
    }
    return false;
}

bool OccupancyGrid3D::get_ground_positions(glm::vec3 pos1, glm::vec3 pos2, std::vector<glm::ivec3>& output, int max_step_up, int max_drop, int max_y_diff, bool allow_flying_over_precepices) {
    // LOG_METHOD();

    std::vector<glm::ivec3> line_positions = line_intersects_xz(pos1, pos2);
    
    if (!adjust_to_ground(line_positions, max_step_up, max_drop, max_y_diff, allow_flying_over_precepices))
        return false;
    
    for (int i = 0; i < line_positions.size(); i++)
        line_positions[i].y -= 1;

    output.insert(output.end(), line_positions.begin(), line_positions.end());

    return true;
}

bool OccupancyGrid3D::get_ground_positions(std::vector<glm::vec3> polyline, std::vector<glm::ivec3>& output, int max_step_up, int max_drop, int max_y_diff, bool allow_flying_over_precepices) {
    // LOG_METHOD();
    
    if (polyline.size() < 2)
        return false;
       
    for (int i = 0; i < polyline.size() - 1; i++) {
        if (!get_ground_positions(polyline[i], polyline[i+1], output, max_step_up, max_drop, max_y_diff, allow_flying_over_precepices))
            return false;
    }
    
    return true;
}

bool OccupancyGrid3D::get_ground_positions(std::vector<NonholonomicPos> polyline, std::vector<glm::ivec3>& output, int max_step_up, int max_drop, int max_y_diff, bool allow_flying_over_precepices) {
    // LOG_METHOD();
    
    if (polyline.size() < 2)
        return false;
       
    for (int i = 0; i < polyline.size() - 1; i++) {
        if (!get_ground_positions(polyline[i].pos, polyline[i+1].pos, output, max_step_up, max_drop, max_y_diff, allow_flying_over_precepices))
            return false;
    }
    
    return true;
}
