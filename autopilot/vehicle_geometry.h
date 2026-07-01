#pragma once

#include <filesystem>
#include <string>

#include <glm/vec3.hpp>

struct VehicleGeometry {
    std::string name;

    glm::vec3 size{0.0f}; // width, height, length

    float axle_track_width = 0.0f;
    float wheel_radius = 0.0f;
    float rear_axle_from_rear = 0.0f;
    float front_axle_from_rear = 0.0f;

    float lidar_from_rear = 0.0f;
    float lidar_height = 0.0f;
    float lidar_from_left = 0.0f;

    glm::vec3 rear_axle_midpoint() const;
    glm::vec3 front_axle_midpoint() const;
    glm::vec3 lidar_position() const;
};

VehicleGeometry load_vehicle_geometry(const std::filesystem::path& path);
