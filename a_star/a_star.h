#pragma once

#include <glm/glm.hpp>
#include <queue>
#include <vector>
#include <set>
#include <iostream>
#include <algorithm>

#include "occupancy_grid_3d.h"
#include "../math_utils.h"
// #include "../voxel_engine/voxel_grid.h"
// #include "voxel_occupancy_grid_3d.h"

class VoxelGrid;

class AStar {
public:
    const int max_step_up = 2;
    const int max_drop = 6;
    const int max_y_diff = 1;
    const int limit = 20000;
    bool allow_diagonal_moves = false;
    bool allow_flying_over_precepices = true;
    bool use_straight_fallback = true;
    uint32_t try_straight_interval = 100;

    
    // AStar();
    AStar(VoxelGrid& voxel_grid);

    virtual float get_heuristic(glm::ivec3 a, glm::ivec3 b);
    std::vector<glm::ivec3> get_straight_path(glm::ivec3& start, glm::ivec3& end, std::vector<glm::ivec3>& out_path);
    bool try_straight_shot(glm::ivec3& start, glm::ivec3& end, std::vector<glm::ivec3>& out_path);

    virtual PlainAstarData reconstruct_path(std::unordered_map<uint64_t, AStarCell> closed_heap, glm::ivec3 pos);
    // bool adjust_to_ground(glm::ivec3& voxel_pos, int max_step_up = 1, int max_drop = 1);
    // virtual std::vector<glm::ivec3> find_path(glm::ivec3 start_pos, glm::ivec3 end_pos);
    virtual PlainAstarData find_path(glm::ivec3 start_pos, glm::ivec3 end_pos);

    

    OccupancyGrid3D& occupancy_grid() noexcept;

protected:
    OccupancyGrid3D m_grid;
};