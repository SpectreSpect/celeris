#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include <cmath>

struct NonholonomicPos {
    glm::vec3 pos = glm::vec3(0.0f, 0.0f, 0.0f);
    float theta = 0; // orientation
    float dubins_segment_id = 1;
    // bool past_dubins_segment = false;
    // bool past_plain_astar_segment = false;

    float steer = 0;
    float dir = 1;
};