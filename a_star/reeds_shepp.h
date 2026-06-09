#pragma once
#include "../nonholonomic_pos.h"
#include <vector>
#include <array>
#include <functional>

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

class ReedsShepp {
public:
    ReedsShepp() = default;
    // std::vector<NonholonomicPos> find_path(NonholonomicPos& start, NonholonomicPos& end);
    static glm::vec3 change_of_basis_r(glm::vec3& p1, glm::vec3& p2, float min_radius);
    static NonholonomicPos change_of_basis_r(NonholonomicPos& p1, NonholonomicPos& p2, float min_radius);
    static glm::vec2 cart_to_polar(glm::vec2 pos);
    static NonholonomicPathElement create_path_element(float dist, Steering steering, Gear gear);
    static float wrap_pi(float theta);
    static std::vector<NonholonomicPos> discretize_path(NonholonomicPos start_pos, std::vector<NonholonomicPathElement> path, int points_per_circlar_element, float min_radius = 1.0f);
    static std::vector<NonholonomicPathElement> timeflip(std::vector<NonholonomicPathElement> path);
    static std::vector<NonholonomicPathElement> reflect(std::vector<NonholonomicPathElement> path);
    static void remove_zero_segments(std::vector<NonholonomicPathElement>& path, float eps = 1e-6f);
    static float get_length(std::vector<NonholonomicPathElement>& path);
    static std::vector<std::vector<NonholonomicPathElement>> get_all_paths(NonholonomicPos start, NonholonomicPos end, float min_radius);
    static std::vector<NonholonomicPathElement> get_optimal_path(NonholonomicPos start, NonholonomicPos end, float min_radius);
    static std::vector<NonholonomicPos> get_optimal_path_discretized(NonholonomicPos start, NonholonomicPos end, int points_per_circlar_element, float min_radius);
    static std::vector<NonholonomicPathElement> path1(float x, float y, float phi);
    static std::vector<NonholonomicPathElement> path2(float x, float y, float phi);
    static std::vector<NonholonomicPathElement> path3(float x, float y, float phi);
    static std::vector<NonholonomicPathElement> path4(float x, float y, float phi);
    static std::vector<NonholonomicPathElement> path5(float x, float y, float phi);
    static std::vector<NonholonomicPathElement> path6(float x, float y, float phi);
    static std::vector<NonholonomicPathElement> path7(float x, float y, float phi);
    static std::vector<NonholonomicPathElement> path8(float x, float y, float phi);
    static std::vector<NonholonomicPathElement> path9(float x, float y, float phi);
    static std::vector<NonholonomicPathElement> path10(float x, float y, float phi);
    static std::vector<NonholonomicPathElement> path11(float x, float y, float phi);
    static std::vector<NonholonomicPathElement> path12(float x, float y, float phi);

    static std::vector<std::vector<NonholonomicPathElement>> get_all_dubins_paths(NonholonomicPos start, NonholonomicPos end, float min_radius);
    static std::vector<NonholonomicPathElement> get_optimal_dubins_path(NonholonomicPos start, NonholonomicPos end, float min_radius);
    static std::vector<NonholonomicPos> get_optimal_dubins_path_discretized(
        NonholonomicPos start, NonholonomicPos end,
        int points_per_circlar_element, float min_radius
    );
    static float mod2pi(float a);
    static bool dubins_LSL(float alpha, float beta, float d, float& t, float& p, float& q);
    static bool dubins_RSR(float alpha, float beta, float d, float& t, float& p, float& q);
    static bool dubins_LSR(float alpha, float beta, float d, float& t, float& p, float& q);
    static bool dubins_RSL(float alpha, float beta, float d, float& t, float& p, float& q);
    static bool dubins_RLR(float alpha, float beta, float d, float& t, float& p, float& q);
    static bool dubins_LRL(float alpha, float beta, float d, float& t, float& p, float& q);

};