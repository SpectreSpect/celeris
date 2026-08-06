#pragma once

#include <chrono>

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#define TYPE_LIDAR 0
#define TYPE_IMU 1

struct Odometry {
    glm::vec3 linear_acceleration = glm::vec3(0, 0, 0);
    glm::vec3 linear_velocity = glm::vec3(0, 0, 0);

    glm::vec3 angular_velocity = glm::vec3(0, 0, 0);

    glm::vec3 position = glm::vec3(0, 0, 0);
    glm::quat orientation{1.0f, 0.0f, 0.0f, 0.0f};

    std::int64_t timestamp_ns = 0;

    int sensor_type = TYPE_LIDAR;
};