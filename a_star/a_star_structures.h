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
    int dubins_segment_id = 1;

    float steer = 0;
    float dir = 1;
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
