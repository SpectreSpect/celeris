#pragma once
#include "a_star.h"
#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include <cmath>
#include "../line.h"


class CurvatureAStar : public AStar {
public:
    const float curvature_limit = 0.2f;

    virtual PlainAstarData reconstruct_path(std::unordered_map<uint64_t, AStarCell> closed_heap, glm::ivec3 pos) override;
    PlainAstarData find_path(glm::ivec3 start_pos, glm::ivec3 end_pos) override;
};