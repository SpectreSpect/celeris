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

    void clear_cache();

    static glm::ivec3 floor_pos(const glm::vec3& p);
    static std::vector<glm::ivec3> line_intersects(glm::vec3 pos1, glm::vec3 pos2);
    bool is_solid(glm::ivec3 pos);
    bool check_footprint(glm::ivec3 origin, glm::ivec3 offsets, uint32_t max_step_up);
    bool adjust_to_ground(
        std::vector<glm::vec3>& output, 
        int max_step_up = 500, 
        int max_drop = 500, 
        int max_y_diff = -1, 
        bool allow_flying_over_precepices = true
    );
    bool adjust_to_ground(
        std::vector<glm::ivec3>& output, 
        int max_step_up = 500, 
        int max_drop = 500, 
        int max_y_diff = -1, 
        bool allow_flying_over_precepices = true
    );
    bool adjust_to_ground(
        std::vector<NonholonomicPos>& output, 
        int max_step_up = 500, 
        int max_drop = 500, 
        int max_y_diff = -1, 
        bool allow_flying_over_precepices = true
    );
    bool adjust_to_ground(
        glm::vec3& output, 
        int max_step_up = 500, 
        int max_drop = 500, 
        int max_y_diff = -1, 
        bool allow_flying_over_precepices = true, 
        uint32_t* status = nullptr
    );
    bool get_closest_invisible_top_pos(glm::ivec3 pos, glm::ivec3 &result, int scan_height);
    bool get_closest_visible_bottom_pos(glm::ivec3 pos, glm::ivec3 &result, int max_drop);
    bool get_ground_positions(
        glm::vec3 pos1, 
        glm::vec3 pos2, 
        std::vector<glm::ivec3>& output, 
        int max_step_up = 500, 
        int max_drop = 500, 
        int max_y_diff = -1,
        bool allow_flying_over_precepices = false
    );
    bool get_ground_positions(
        std::vector<glm::vec3> polyline, 
        std::vector<glm::ivec3>& output, 
        int max_step_up = 500, 
        int max_drop = 500, 
        int max_y_diff = -1,
        bool allow_flying_over_precepices = false
    );
    bool get_ground_positions(
        std::vector<NonholonomicPos> polyline, 
        std::vector<glm::ivec3>& output, 
        int max_step_up = 500, 
        int max_drop = 500, 
        int max_y_diff = -1,
        bool allow_flying_over_precepices = false
    );
    std::vector<glm::ivec3> line_intersects_xz(glm::vec3 pos1, glm::vec3 pos2);

    template <class T, class GetPos, class SetPos>
    bool adjust_to_ground_range(
        T* begin, 
        T* end,
        GetPos get_pos, 
        SetPos set_pos,
        int max_step_up, 
        int max_drop, 
        int max_y_diff, 
        bool allow_flying_over_precepices = true)
    {
        if (begin == end) return true; 

        float last_y = get_pos(*begin).y;

        for (auto* it = begin; it != end; ++it) {
            glm::vec3 p = get_pos(*it);
            p.y = last_y;

            if (!adjust_to_ground(p, max_step_up, max_drop, max_y_diff, allow_flying_over_precepices))
                return false;

            set_pos(*it, p);
            last_y = p.y;
        }
        return true;
    }
    
private:
    VoxelGrid* m_voxel_grid = nullptr;
    std::unordered_map<uint64_t, VoxelGridChunk> m_chunk_cache;
    std::unordered_map<uint64_t, bool> m_is_chunk_read;
};
