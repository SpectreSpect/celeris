#pragma once

#include <glm/vec3.hpp>

struct LidarMessagePointData {
    glm::vec3 position;
    uint64_t time_offset_ns;
};