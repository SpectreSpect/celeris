#pragma once

#include <glm/glm.hpp>
#include <vector>

class VehicleBase {
public:
    struct PointProjection {
        float s = 0.0f;
        float dist = 0.0f;
        glm::vec2 point = glm::vec2{0.0f};
        glm::vec2 tangent = glm::vec2{1.0f, 0.0f};
        float heading = 0.0f;
        float dir = 1.0f;
    };

    struct PathArcLengthTable {
        std::vector<float> point_s;
        std::vector<float> direction_change_s;
        float total_length = 0.0f;
    };

    struct VehicleTransformState {
        glm::vec2 position = glm::vec2{0.0f};
        float speed = 0.0f;
        float speed_acceleration = 0.0f;
        float heading = 0.0f;

        float steering_angle = 0.0f;
        float steering_angle_velocity = 0.0f;
        float steering_angle_acceleration = 0.0f;
    };

public:
    virtual ~VehicleBase() noexcept = default;

    VehicleBase(const VehicleBase&) = delete;
    VehicleBase& operator=(const VehicleBase&) = delete;

    VehicleBase(VehicleBase&&) noexcept = default;
    VehicleBase& operator=(VehicleBase&&) noexcept = default;

    virtual VehicleTransformState& state() noexcept = 0;
    virtual const VehicleTransformState& state() const noexcept = 0;

    virtual void vehicle_simulation_step(
        VehicleTransformState& state,
        float speed_acceleration,
        float steering_control,
        float dt = 0.05f,
        bool debug = true
    ) const = 0;

    virtual VehicleTransformState get_vehicle_simulation_step(
        const VehicleTransformState& state,
        float speed_acceleration,
        float steering_control,
        float dt = 0.05f,
        bool debug = true
    ) const = 0;

    virtual void vehicle_simulation_step(
        float speed_acceleration,
        float steering_control,
        float dt = 0.05f,
        bool debug = true
    ) = 0;

    virtual VehicleTransformState get_vehicle_simulation_step(
        float speed_acceleration,
        float steering_control,
        float dt = 0.05f,
        bool debug = true
    ) = 0;

    virtual void simulate_vehicle(
        float speed_acceleration,
        float steering_control,
        float simulation_time,
        float dt = 0.05f,
        bool debug = true
    ) = 0;

protected:
    VehicleBase() noexcept = default;
};
