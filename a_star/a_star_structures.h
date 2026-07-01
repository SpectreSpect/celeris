#pragma once

#include <cmath>
#include <glm/glm.hpp>
#include <vector>

#include "../renderer/transform.h"
#include "../vulkan_self/utils.h"

struct AStarCell {
    float g;
    float f;
    glm::ivec3 pos;
    glm::ivec3 came_from;
    bool no_parent = true;

    bool has_intermediate_pos = false;
    glm::ivec3 intermediate_pos;
};

// struct ByPriority {
//     bool operator()(const AStarCell& a, const AStarCell& b) const {
//         return a.f > b.f; // higher priority first
//     }
// };

struct ByPriority {
    bool operator()(const AStarCell& a, const AStarCell& b) const {
        if (a.f != b.f)
            return a.f > b.f; // Lower f first

        return a.g < b.g;     // On ties, larger g first
    }
};

struct PlainAstarData {
    std::vector<glm::ivec3> path;
    std::vector<float> dist_to_end;
    bool reached_precipice = false;
};

struct NonholonomicPos {
    glm::vec3 pos = glm::vec3(0.0f, 0.0f, 0.0f);
    float theta = 0; // orientation
    int dubins_segment_id = 1;

    float steer = 0;
    float dir = 1;

    void from_transform(const Transform& transform) {
        glm::quat q = glm::normalize(transform.rotation);
        glm::vec3 forward = q * glm::vec3(-1.0f, 0.0f, 0.0f);
        
        pos = transform.position;
        theta = std::atan2(forward.z, forward.x);
    }
};

struct NonholonomicAStarCell {
    float g;
    float f;
    NonholonomicPos pos;
    NonholonomicPos came_from;
    bool no_parent = true;
};

struct NonholonomicByPriority {
    bool operator()(const NonholonomicAStarCell& a, const NonholonomicAStarCell& b) const {
        return a.f > b.f; // higher priority first
    }
};

struct DistToPathData {
    float dist;
    int id;
};

enum class Steering : int {
    LEFT = -1,
    RIGHT = 1,
    STRAIGHT = 0
};

enum class Gear : int {
    FORWARD = 1,
    BACKWARD = -1
};


class NonholonomicPathElement {
public:
    float dist = 0;
    Steering steering = Steering::STRAIGHT;
    Gear gear = Gear::FORWARD;

    NonholonomicPathElement(float dist = 0, Steering steering = Steering::STRAIGHT, Gear gear = Gear::FORWARD) {
        this->dist = dist;
        this->steering = steering;
        this->gear = gear;

        if (this->dist < 0) {
            this->dist = -this->dist;
            reverse_gear();
        }
    }

    void reverse_steering() {
        if (steering == Steering::LEFT) steering = Steering::RIGHT;
        else if (steering == Steering::RIGHT) steering = Steering::LEFT;
    }

    void reverse_gear() {
        gear = (gear == Gear::FORWARD) ? Gear::BACKWARD : Gear::FORWARD;
    }
};

struct VehiclePathPoint {
    glm::vec2 position = glm::vec2{0.0f};
    float heading = 0.0f;
    float dir = 1.0f;
};

inline std::vector<VehiclePathPoint> make_forward_vehicle_path(
    const std::vector<glm::vec2>& polyline)
{
    std::vector<VehiclePathPoint> path;
    path.reserve(polyline.size());

    for (glm::vec2 position : polyline) {
        path.push_back(VehiclePathPoint{
            .position = position,
            .heading = 0.0f,
            .dir = 1.0f
        });
    }

    float last_heading = 0.0f;
    for (size_t i = 0; i < path.size(); i++) {
        bool found_heading = false;

        for (size_t j = i + 1; j < path.size(); j++) {
            const glm::vec2 segment = path[j].position - path[i].position;
            if (glm::length(segment) > Utils::eps) {
                last_heading = std::atan2(segment.y, segment.x);
                found_heading = true;
                break;
            }
        }

        if (!found_heading && i > 0) {
            for (size_t j = i; j > 0; j--) {
                const glm::vec2 segment = path[j].position - path[j - 1].position;
                if (glm::length(segment) > Utils::eps) {
                    last_heading = std::atan2(segment.y, segment.x);
                    break;
                }
            }
        }

        path[i].heading = last_heading;
    }

    return path;
}

inline std::vector<VehiclePathPoint> make_vehicle_path(
    const std::vector<NonholonomicPos>& nonholonomic_path)
{
    std::vector<VehiclePathPoint> path;
    path.reserve(nonholonomic_path.size());

    for (const NonholonomicPos& point : nonholonomic_path) {
        path.push_back(VehiclePathPoint{
            .position = glm::vec2{point.pos.x, point.pos.z},
            .heading = point.theta,
            .dir = point.dir < 0.0f ? -1.0f : 1.0f
        });
    }

    return path;
}
