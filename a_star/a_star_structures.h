#pragma once

#include <glm/glm.hpp>

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

struct NonholonomicPos {
    glm::vec3 pos = glm::vec3(0.0f, 0.0f, 0.0f);
    float theta = 0; // orientation
    float dubins_segment_id = 1;

    float steer = 0;
    float dir = 1;
};