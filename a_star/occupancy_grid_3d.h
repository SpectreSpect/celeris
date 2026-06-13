#pragma once

#include <unordered_set>
#include <vector>
#include "../voxel_grid_vulkan/voxel_grid.h"
#include "a_star_structures.h"
#include "../vulkan_self/logger/logger_header.h"

class OccupancyGrid3D {
public:
    _XCLASS_NAME(OccupancyGrid3D);

    OccupancyGrid3D(VoxelGrid& voxel_grid);

    static glm::ivec3 floor_pos(const glm::vec3& p);
    static std::vector<glm::ivec3> line_intersects(glm::vec3 pos1, glm::vec3 pos2);
    bool is_solid(glm::ivec3 pos);
    bool adjust_to_ground(std::vector<glm::vec3>& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1, bool allow_flying_over_precepices = true);
    bool adjust_to_ground(std::vector<glm::ivec3>& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1, bool allow_flying_over_precepices = true);
    bool adjust_to_ground(std::vector<NonholonomicPos>& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1, bool allow_flying_over_precepices = true);
    bool adjust_to_ground(glm::vec3& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1, bool allow_flying_over_precepices = true, uint32_t* status = nullptr);
    bool get_closest_invisible_top_pos(glm::ivec3 pos, glm::ivec3 &result, int scan_height);
    bool get_closest_visible_bottom_pos(glm::ivec3 pos, glm::ivec3 &result, int max_drop);
    bool get_ground_positions(glm::vec3 pos1, glm::vec3 pos2, std::vector<glm::ivec3>& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1);
    bool get_ground_positions(std::vector<glm::vec3> polyline, std::vector<glm::ivec3>& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1);
    bool get_ground_positions(std::vector<NonholonomicPos> polyline, std::vector<glm::ivec3>& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1);
    std::vector<glm::ivec3> line_intersects_xz(glm::vec3 pos1, glm::vec3 pos2);

    template <class T, class GetPos, class SetPos>
    bool adjust_to_ground_range(T* begin, T* end,
                                        GetPos get_pos, SetPos set_pos,
                                        int max_step_up, int max_drop, int max_y_diff, bool allow_flying_over_precepices = true)
    {
        if (begin == end) return true; 

        float last_y = get_pos(*begin).y;

        for (auto* it = begin; it != end; ++it) {
            glm::vec3 p = get_pos(*it);
            p.y = last_y;

            if (!adjust_to_ground(p, max_step_up, max_drop, max_y_diff, allow_flying_over_precepices))
                return false;

            // if (max_y_diff >= 0 && std::abs(get_pos(*it).y - p.y) > max_y_diff)
            //     return false;

            set_pos(*it, p);
            last_y = p.y;
        }
        return true;
    }
    
    // template <class It, class GetPos, class Fn>
    // bool for_each_ground_cell_on_polyline_xz(It begin, It end,
    //                                         GetPos getPos,
    //                                         int max_step_up, int max_drop, int max_y_diff,
    //                                         Fn&& fn) const
    // {
    //     if (begin == end) return false;
    //     It it0 = begin;
    //     It it1 = std::next(begin);
    //     if (it1 == end) return false;

    //     glm::ivec3 last_sent(0);
    //     bool have_last = false;

    //     for (; it1 != end; ++it0, ++it1) {
    //         glm::vec3 p0 = getPos(*it0);
    //         glm::vec3 p1 = getPos(*it1);

    //         const int y_layer = (int)std::floor(p0.y);

    //         bool ok_seg = detail_for_each_cell_on_line_xz(p0, p1, [&](const glm::ivec2& c2) -> bool {
    //             // Probe position in this XZ cell at the segment's y layer
    //             glm::vec3 probe((float)c2.x, (float)y_layer, (float)c2.y);

    //             // Adjust probe.y to the first "invisible" top position (your convention)
    //             if (!const_cast<Grid3D*>(this)->adjust_to_ground(probe, max_step_up, max_drop, max_y_diff))
    //                 return false;

    //             // Convert to ground cell (same as your get_ground_positions(): y -= 1)
    //             glm::ivec3 ground(c2.x, (int)probe.y - 1, c2.y);

    //             // Skip trivial duplicates at polyline joints
    //             if (have_last && ground == last_sent) return true;
    //             last_sent = ground;
    //             have_last = true;

    //             // User callback decides whether to keep going
    //             return fn(ground);
    //         });

    //         if (!ok_seg) return false;
    //     }

    //     return true;
    // }

    // Convenience overload for vector<glm::vec3>
    template <class Fn>
    bool for_each_ground_cell_on_polyline_xz(const std::vector<glm::vec3>& polyline,
                                            int max_step_up, int max_drop, int max_y_diff,
                                            Fn&& fn) const
    {
        return for_each_ground_cell_on_polyline_xz(
            polyline.begin(), polyline.end(),
            [](const glm::vec3& p) { return p; },
            max_step_up, max_drop, max_y_diff,
            std::forward<Fn>(fn)
        );
    }

private:
    VoxelGrid* m_voxel_grid = nullptr;

    std::unordered_map<uint64_t, VoxelGridChunk> m_chunk_cache;
};
