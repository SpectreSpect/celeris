#pragma once

#include <cstdint>
#include <glm/vec3.hpp>

struct ImuMeasurement {
    glm::vec3 linear_acceleration;
    glm::vec3 angular_velocity;
    std::int64_t timestamp;
};