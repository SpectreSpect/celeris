#pragma once
#include "occupancy_grid_3d.h"
#include <queue>
#include <vector>
#include <set>
#include <iostream>
#include <algorithm>
#include "../voxel_engine/voxel_grid.h"
#include "voxel_occupancy_grid_3d.h"

struct AStarCell {
    float g;
    float f;
    glm::ivec3 pos;
    glm::ivec3 came_from;
    bool no_parent = true;

    bool has_intermediate_pos = false;
    glm::ivec3 intermediate_pos;
};

struct ByPriority {
    bool operator()(const AStarCell& a, const AStarCell& b) const {
        return a.f > b.f; // higher priority first
    }
};

struct PlainAstarData {
    std::vector<glm::ivec3> path;
    std::vector<float> dist_to_end;
};

class AStar {
public:
    const int max_step_up = 500;
    const int max_drop = 500;
    const int max_y_diff = 1;
    bool allow_diagonal_moves = false;

    OccupancyGrid3D* grid;
    AStar();
    AStar(VoxelGrid* voxel_grid);

    virtual float get_heuristic(glm::ivec3 a, glm::ivec3 b);

    virtual PlainAstarData reconstruct_path(std::unordered_map<uint64_t, AStarCell> closed_heap, glm::ivec3 pos);
    // bool adjust_to_ground(glm::ivec3& voxel_pos, int max_step_up = 1, int max_drop = 1);
    // virtual std::vector<glm::ivec3> find_path(glm::ivec3 start_pos, glm::ivec3 end_pos);
    virtual PlainAstarData find_path(glm::ivec3 start_pos, glm::ivec3 end_pos);
};