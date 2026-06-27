#pragma once

#include <mutex>
#include <unordered_set>
#include <vector>

#include "../vulkan_self/pass/instance/pass_instance.h"
#include "../vulkan_self/logger/logger_header.h"
#include "../voxel_grid_vulkan/voxel_grid.h"
#include "a_star_structures.h"

#include "../utils/avg_timer.h"

class OccupancyGrid3D {
public:
    _XCLASS_NAME(OccupancyGrid3D);

    OccupancyGrid3D(
        VulkanPhysicalDevice& physical_device, 
        VulkanDevice& device, 
        VoxelGrid& voxel_grid, 
        ComputePassManager& compute_pass_manager
    );

    void clear_cache();

    static glm::ivec3 floor_pos(const glm::vec3& p);
    static std::vector<glm::ivec3> line_intersects(glm::vec3 pos1, glm::vec3 pos2);
    glm::vec3 voxel_size() const;
    glm::ivec3 world_to_voxel_pos(const glm::vec3& p) const;
    glm::vec3 voxel_to_world_pos(const glm::ivec3& p) const;
    glm::vec3 voxel_center_world_pos(const glm::ivec3& p) const;
    bool is_solid(glm::ivec3 pos);
    bool check_footprint(glm::vec3 origin, glm::vec3 offsets, uint32_t max_step_up);
    bool adjust_to_ground(
        glm::ivec3& output,
        int max_step_up = 500,
        int max_drop = 500,
        int max_y_diff = -1,
        bool allow_flying_over_precepices = true,
        uint32_t* status = nullptr
    );
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

    AvgTimer read_and_inflate_chunk_time;
    AvgTimer is_solid_time;
    uint32_t read_and_inflate_chunk_count = 0;
    uint32_t is_solid_count = 0;
    
private:
    VoxelGrid* m_voxel_grid = nullptr;

    PassInstance m_prepare_copy_dirty_list_dispatch_args_pi;
    PassInstance m_copy_dirty_list_pi;
    VulkanBuffer m_dirty_chunk_position_buffer;

    std::unordered_map<uint64_t, VoxelGridChunk> m_chunk_cache;
    std::mutex m_chunk_cache_mutex;
    std::vector<glm::vec4> m_dirty_chunk_positions;
};
