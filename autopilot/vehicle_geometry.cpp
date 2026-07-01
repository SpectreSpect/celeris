#include "vehicle_geometry.h"

#include <stdexcept>

#include <yaml-cpp/yaml.h>

namespace {
YAML::Node require_node(const YAML::Node& parent, const char* key) {
    YAML::Node node = parent[key];
    if (!node) {
        throw std::runtime_error(std::string("Missing vehicle config key: ") + key);
    }

    return node;
}

float require_float(const YAML::Node& parent, const char* key) {
    return require_node(parent, key).as<float>();
}
}

glm::vec3 VehicleGeometry::rear_axle_midpoint() const {
    return glm::vec3(size.x / 2.0f, wheel_radius, rear_axle_from_rear);
}

glm::vec3 VehicleGeometry::front_axle_midpoint() const {
    return glm::vec3(size.x / 2.0f, wheel_radius, front_axle_from_rear);
}

glm::vec3 VehicleGeometry::lidar_position() const {
    return glm::vec3(lidar_from_left, lidar_height, lidar_from_rear);
}

VehicleGeometry load_vehicle_geometry(const std::filesystem::path& path) {
    YAML::Node config = YAML::LoadFile(path.string());
    YAML::Node size = require_node(config, "size");
    YAML::Node axles = require_node(config, "axles");
    YAML::Node lidar = require_node(config, "lidar");

    VehicleGeometry geometry;
    geometry.name = require_node(config, "name").as<std::string>();

    geometry.size = glm::vec3(
        require_float(size, "width"),
        require_float(size, "height"),
        require_float(size, "length")
    );

    geometry.axle_track_width = require_float(axles, "track_width");
    geometry.wheel_radius = require_float(axles, "wheel_radius");
    geometry.rear_axle_from_rear = require_float(axles, "rear_from_rear");
    geometry.front_axle_from_rear = require_float(axles, "front_from_rear");

    geometry.lidar_from_rear = require_float(lidar, "from_rear");
    geometry.lidar_height = require_float(lidar, "height");
    geometry.lidar_from_left = require_float(lidar, "from_left");

    return geometry;
}
