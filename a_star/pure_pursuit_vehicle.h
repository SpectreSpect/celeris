#pragma once

#include <glm/glm.hpp>
#include <limits>
#include <vector>

#include "../vulkan_self/logger/logger_header.h"
#include "a_star_structures.h"
#include "vehicle.h"

class PurePursuitVehicle : public VehicleBase {
public:
    _XCLASS_NAME(PurePursuitVehicle);

public:
    using VehicleBase::PathArcLengthTable;
    using VehicleBase::PointProjection;
    using VehicleBase::VehicleTransformState;

    struct PurePursuitParams {
        float cruise_speed = 12.0f;
        float lookahead_distance = 7.0f;
        float lookahead_speed_gain = 0.8f;
        float min_lookahead_distance = 4.0f;
        float max_lookahead_distance = 16.0f;
        float min_steering_lookahead_distance = 5.0f;
        float goal_steering_release_distance = 3.0f;
        float goal_steering_release_speed = 1.5f;
        float speed_p_gain = 1.0f;
        float steering_p_gain = 2.2f;
        float slowdown_distance_from_path = 3.5f;
        float min_off_path_speed_factor = 0.25f;
        float projection_backtrack_window = 1.0f;
        float projection_lookahead_base = 8.0f;
        float direction_switch_arrival_distance = 0.5f;
        float direction_switch_arrival_speed = 1.5f;
        float direction_switch_approach_distance = 6.0f;
        float direction_switch_approach_speed = 2.0f;
    };

    struct PurePursuitControlCommand {
        float speed_acceleration = 0.0f;
        float steering_angle_velocity = 0.0f;
    };

    struct PurePursuitStepResult {
        PurePursuitControlCommand control_command;
        VehicleTransformState state_after_step;
        PointProjection current_projection;
        PointProjection target_projection;
        float lookahead_distance = 0.0f;
        float target_steering_angle = 0.0f;
        float target_speed = 0.0f;
    };

public:
    PurePursuitVehicle(
        const VehicleTransformState& initial_state,
        float max_acceleration,
        float max_steering_angle_velocity,
        float wheel_base = 2.5f
    );

    PurePursuitVehicle(
        float max_acceleration,
        float max_steering_angle_velocity,
        float wheel_base = 2.5f
    );
    ~PurePursuitVehicle() noexcept override = default;

    PurePursuitVehicle(const PurePursuitVehicle&) = delete;
    PurePursuitVehicle& operator=(const PurePursuitVehicle&) = delete;

    PurePursuitVehicle(PurePursuitVehicle&&) noexcept = default;
    PurePursuitVehicle& operator=(PurePursuitVehicle&&) noexcept = default;

    VehicleTransformState& state() noexcept override;
    const VehicleTransformState& state() const noexcept override;
    PurePursuitParams& params() noexcept;
    const PurePursuitParams& params() const noexcept;
    void reset_params() noexcept;

    void vehicle_simulation_step(
        VehicleTransformState& state,
        float speed_acceleration,
        float steering_angle_velocity,
        float dt = 0.05f,
        bool debug = true
    ) const override;

    VehicleTransformState get_vehicle_simulation_step(
        const VehicleTransformState& state,
        float speed_acceleration,
        float steering_angle_velocity,
        float dt = 0.05f,
        bool debug = true
    ) const override;

    void vehicle_simulation_step(
        float speed_acceleration,
        float steering_angle_velocity,
        float dt = 0.05f,
        bool debug = true
    ) override;

    VehicleTransformState get_vehicle_simulation_step(
        float speed_acceleration,
        float steering_angle_velocity,
        float dt = 0.05f,
        bool debug = true
    ) override;

    void simulate_vehicle(
        float speed_acceleration,
        float steering_angle_velocity,
        float simulation_time,
        float dt = 0.05f,
        bool debug = true
    ) override;

    PurePursuitStepResult follow_polyline_step(
        VehicleTransformState& state,
        const std::vector<glm::vec2>& polyline,
        float dt = 0.05f
    ) const;

    PurePursuitStepResult follow_polyline_step(
        VehicleTransformState& state,
        const std::vector<VehiclePathPoint>& path,
        float dt = 0.05f
    ) const;

    PurePursuitStepResult follow_polyline_step(
        VehicleTransformState& state,
        const std::vector<VehiclePathPoint>& path,
        const PathArcLengthTable& path_arc_lengths,
        float projection_min_s,
        float projection_max_s,
        float dt = 0.05f
    ) const;

    PurePursuitStepResult follow_polyline_step(
        const std::vector<glm::vec2>& polyline,
        float dt = 0.05f
    );

    PurePursuitStepResult follow_polyline_step(
        const std::vector<VehiclePathPoint>& path,
        float dt = 0.05f
    );

    PurePursuitControlCommand compute_control(
        const VehicleTransformState& state,
        const std::vector<VehiclePathPoint>& path,
        const PathArcLengthTable& path_arc_lengths,
        float projection_min_s,
        float projection_max_s,
        PointProjection* current_projection = nullptr,
        PointProjection* target_projection = nullptr,
        float* lookahead_distance = nullptr,
        float* target_steering_angle = nullptr,
        float* target_speed = nullptr
    ) const;

private:
    VehicleTransformState m_vehicle_state;
    float m_max_acceleration;
    float m_max_steering_angle_velocity;
    float m_wheel_base;
    PurePursuitParams m_params;

    void check_simulation_step(
        float speed_acceleration,
        float steering_angle_velocity,
        float dt
    ) const;

    void simulate_vehicle(
        VehicleTransformState& state,
        float speed_acceleration,
        float steering_angle_velocity,
        float simulation_time,
        float dt = 0.05f,
        bool debug = true
    ) const;
};
