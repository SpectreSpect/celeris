#pragma once

#include <glm/glm.hpp>

struct NonholonomicPos {
    glm::vec3 pos = glm::vec3(0.0f, 0.0f, 0.0f);
    float theta = 0; // orientation
    float dubins_segment_id = 1;

    float steer = 0;
    float dir = 1;
};