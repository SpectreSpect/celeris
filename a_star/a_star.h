#pragma once

#include <glm/glm.hpp>
#include <queue>
#include <vector>
#include <set>
#include <iostream>
#include <algorithm>

#include "occupancy_grid_3d.h"
#include "../math_utils.h"

#include "../vulkan_self/logger/logger_header.h"

class VoxelGrid;

class AStar {
public:
    _XCLASS_NAME(AStar);

    struct AStarParams {
        int max_step_up = 2;
        int max_drop = 6;
        int max_y_diff = 1;
        int iteration_limit = 20000;
        bool allow_diagonal_moves = false;
        bool allow_flying_over_precepices = true;
        bool use_straight_fallback = true;
        uint32_t try_straight_interval = 100;
    };

    typedef AStarParams AStarDesc;

    AStar(OccupancyGrid3D& occupancy_grid, const AStarDesc& desc);

    virtual float get_heuristic(glm::ivec3 a, glm::ivec3 b);
    std::vector<glm::ivec3> get_straight_path(glm::ivec3& start, glm::ivec3& end, std::vector<glm::ivec3>& out_path);
    bool try_straight_shot(glm::ivec3& start, glm::ivec3& end, std::vector<glm::ivec3>& out_path);

    virtual PlainAstarData reconstruct_path(std::unordered_map<uint64_t, AStarCell> closed_heap, glm::ivec3 pos);
    virtual PlainAstarData find_path(glm::ivec3 start_pos, glm::ivec3 end_pos);

    OccupancyGrid3D& occupancy_grid() noexcept;

private:
    OccupancyGrid3D* m_grid = nullptr;
    AStarParams m_params;
};