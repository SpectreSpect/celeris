#pragma once

#include <glm/glm.hpp>
#include <limits>
#include <vector>

#include "../vulkan_self/logger/logger_header.h"
#include "../renderer/transform.h"
#include "a_star_structures.h"
#include "vehicle_base.h"

class Vehicle : public VehicleBase {
public:
    _XCLASS_NAME(Vehicle);

public:
    using VehicleBase::PathArcLengthTable;
    using VehicleBase::PointProjection;
    using VehicleBase::VehicleTransformState;

    struct SimulationControlSearchDesc {
        int speed_acceleration_samples = 7;
        int steer_acceleration_samples = 15;
        int max_results = 5;
        float simulation_time = 2.0f;
        float max_simulation_s = 8.0f;
        float dt = 0.05f;
        float initial_projection_min_s = 0.0f;
        float initial_projection_max_s = std::numeric_limits<float>::infinity();
        bool slow_down_at_projection_max = false;
        float projection_max_target_speed_abs = 0.0f;
        bool debug = false;
    };

    struct SimulationLossWeights {
        float cruise_speed = 1.0f;
        float slowdown_speed = 1.0f;
    };

    struct SimulationFollowParams {
        float cruise_speed = 3.0f;
        float min_slowdown_acceleration = 2.0f;
        float min_off_path_speed_factor = 0.25f;
        float projection_backtrack_window = 0.75f;
        float projection_lookahead_base = 3.0f;
        float segment_switch_radius = 0.35f;
        float min_direction_segment_virtual_length = 3.0f;
        float direction_switch_arrival_speed = 0.9f;
        float direction_switch_approach_speed = 0.75f;
    };

    struct PathPotentialParams {
        float additional_radius = 2.0f;
        float groove_k = 1.0f;
        float plane_k = 0.025f;
    };

    struct VehicleControlCommand {
        float speed_acceleration = 0.0f;
        float steer_acceleration = 0.0f;
    };

    struct SimulationLossBreakdown {
        float position = 0.0f;
        float heading = 0.0f;
        float speed = 0.0f;
        float progress = 0.0f;
        float steering = 0.0f;
        float control = 0.0f;

        float total() const noexcept {
            return position + heading + speed + progress + steering + control;
        }
    };

    struct SimulationControlCandidateDebug {
        float start_s = 0.0f;
        float end_s = 0.0f;
        float target_end_s = 0.0f;
        float start_dist = 0.0f;
        float end_dist = 0.0f;
        float target_end_dist = 0.0f;
        float reference_initial_path_speed = 0.0f;
        float trajectory_length = 0.0f;
        SimulationLossBreakdown loss_breakdown;
        glm::vec2 start_position = glm::vec2{0.0f};
        glm::vec2 end_position = glm::vec2{0.0f};
        glm::vec2 target_end_point = glm::vec2{0.0f};
    };

    struct SimulationControlCandidate {
        VehicleControlCommand control_command;
        float loss = std::numeric_limits<float>::infinity();
        VehicleTransformState predicted_state;
        std::vector<glm::vec2> trajectory;
        SimulationControlCandidateDebug debug;
    };

    struct PolylineFollowStepResult {
        SimulationControlCandidate selected_control;
        VehicleTransformState state_after_step;
    };

public:
    Vehicle(
        const VehicleTransformState& initial_state,
        float max_acceleration,
        float max_steer_acceleration,
        float wheel_base = 2.5f
    );

    Vehicle(
        float max_acceleration,
        float max_steer_acceleration,
        float wheel_base = 2.5f
    );
    ~Vehicle() noexcept override = default;

    Vehicle(const Vehicle&) = delete;
    Vehicle& operator=(const Vehicle&) = delete;

    Vehicle(Vehicle&&) noexcept = default;
    Vehicle& operator=(Vehicle&&) noexcept = default;

    VehicleTransformState& state() noexcept override;
    const VehicleTransformState& state() const noexcept override;
    SimulationLossWeights& loss_weights() noexcept;
    const SimulationLossWeights& loss_weights() const noexcept;
    void reset_loss_weights() noexcept;
    SimulationFollowParams& follow_params() noexcept;
    const SimulationFollowParams& follow_params() const noexcept;
    void reset_follow_params() noexcept;
    float path_potential_distance_exponent() const noexcept;
    void set_path_potential_distance_exponent(float exponent) noexcept;
    PathPotentialParams& path_potential_params() noexcept;
    const PathPotentialParams& path_potential_params() const noexcept;
    void reset_path_potential_params() noexcept;

    void vehicle_simulation_step(
        VehicleTransformState& state,
        float speed_acceleration,
        float steer_acceleration,
        float dt = 0.05f,
        bool debug = true
    ) const override;

    VehicleTransformState get_vehicle_simulation_step(
        const VehicleTransformState& state,
        float speed_acceleration,
        float steer_acceleration,
        float dt = 0.05f,
        bool debug = true
    ) const override;

    void vehicle_simulation_step(
        float speed_acceleration,
        float steer_acceleration,
        float dt = 0.05f,
        bool debug = true
    ) override;

    VehicleTransformState get_vehicle_simulation_step(
        float speed_acceleration,
        float steer_acceleration,
        float dt = 0.05f,
        bool debug = true
    ) override;

    void simulate_vehicle_with_s(
        VehicleTransformState& state,
        float speed_acceleration,
        float steer_acceleration,
        float max_simulation_time,
        float max_simulation_s, 
        float dt = 0.05f,
        bool debug = true
    ) const;

    void simulate_vehicle_with_s(
        float speed_acceleration,
        float steer_acceleration,
        float max_simulation_time,
        float max_simulation_s, 
        float dt = 0.05f,
        bool debug = true
    );

    void simulate_vehicle(
        VehicleTransformState& state,
        float speed_acceleration,
        float steer_acceleration,
        float simulation_time, 
        float dt = 0.05f,
        bool debug = true
    ) const;

    void simulate_vehicle(
        float speed_acceleration,
        float steer_acceleration,
        float simulation_time, 
        float dt = 0.05f,
        bool debug = true
    ) override;

    PolylineFollowStepResult follow_polyline_step(
        VehicleTransformState& state,
        const std::vector<glm::vec2>& polyline
    ) const;

    PolylineFollowStepResult follow_polyline_step(
        VehicleTransformState& state,
        const std::vector<VehiclePathPoint>& path
    ) const;

    PolylineFollowStepResult follow_polyline_step(
        VehicleTransformState& state,
        const std::vector<glm::vec2>& polyline,
        const SimulationControlSearchDesc& desc
    ) const;

    PolylineFollowStepResult follow_polyline_step(
        VehicleTransformState& state,
        const std::vector<VehiclePathPoint>& path,
        const SimulationControlSearchDesc& desc
    ) const;

    PolylineFollowStepResult follow_polyline_step(
        const std::vector<glm::vec2>& polyline
    );

    PolylineFollowStepResult follow_polyline_step(
        const std::vector<VehiclePathPoint>& path
    );

    PolylineFollowStepResult follow_polyline_step(
        const std::vector<glm::vec2>& polyline,
        const SimulationControlSearchDesc& desc
    );

    PolylineFollowStepResult follow_polyline_step(
        const std::vector<VehiclePathPoint>& path,
        const SimulationControlSearchDesc& desc
    );

    std::vector<SimulationControlCandidate> find_best_simulation_controls(
        const VehicleTransformState& state,
        const std::vector<glm::vec2>& polyline
    ) const;

    std::vector<SimulationControlCandidate> find_best_simulation_controls(
        const VehicleTransformState& state,
        const std::vector<VehiclePathPoint>& path
    ) const;

    std::vector<SimulationControlCandidate> find_best_simulation_controls(
        const VehicleTransformState& state,
        const std::vector<glm::vec2>& polyline,
        const SimulationControlSearchDesc& desc
    ) const;

    std::vector<SimulationControlCandidate> find_best_simulation_controls(
        const VehicleTransformState& state,
        const std::vector<VehiclePathPoint>& path,
        const SimulationControlSearchDesc& desc
    ) const;

    std::vector<SimulationControlCandidate> find_best_simulation_controls(
        const VehicleTransformState& state,
        const std::vector<VehiclePathPoint>& path,
        const PathArcLengthTable& path_arc_lengths,
        const SimulationControlSearchDesc& desc
    ) const;

    std::vector<SimulationControlCandidate> find_best_simulation_controls(
        const std::vector<glm::vec2>& polyline
    ) const;

    std::vector<SimulationControlCandidate> find_best_simulation_controls(
        const std::vector<VehiclePathPoint>& path
    ) const;

    std::vector<SimulationControlCandidate> find_best_simulation_controls(
        const std::vector<glm::vec2>& polyline,
        const SimulationControlSearchDesc& desc
    ) const;

    std::vector<SimulationControlCandidate> find_best_simulation_controls(
        const std::vector<VehiclePathPoint>& path,
        const SimulationControlSearchDesc& desc
    ) const;

    std::vector<SimulationControlCandidate> find_best_simulation_controls(
        const std::vector<VehiclePathPoint>& path,
        const PathArcLengthTable& path_arc_lengths,
        const SimulationControlSearchDesc& desc
    ) const;

    static PointProjection find_polyline_projection(
        const std::vector<VehiclePathPoint>& path,
        glm::vec2 point,
        float min_s = 0.0f,
        float max_s = std::numeric_limits<float>::infinity()
    );

    static PointProjection find_polyline_projection(
        const std::vector<VehiclePathPoint>& path,
        const PathArcLengthTable& path_arc_lengths,
        glm::vec2 point,
        float min_s = 0.0f,
        float max_s = std::numeric_limits<float>::infinity()
    );

    static PointProjection sample_polyline_at_s(
        const std::vector<VehiclePathPoint>& path,
        float s
    );

    static PointProjection sample_polyline_at_s(
        const std::vector<VehiclePathPoint>& path,
        const PathArcLengthTable& path_arc_lengths,
        float s
    );

    static PathArcLengthTable build_path_arc_length_table(
        const std::vector<VehiclePathPoint>& path
    );
    static float polyline_length(const std::vector<VehiclePathPoint>& path);
    static float polyline_length(const PathArcLengthTable& path_arc_lengths) noexcept;
    static float next_direction_change_s(
        const std::vector<VehiclePathPoint>& path,
        float s,
        bool include_current = false
    );
    static float next_direction_change_s(
        const std::vector<VehiclePathPoint>& path,
        const PathArcLengthTable& path_arc_lengths,
        float s,
        bool include_current = false
    );

    float evaluate_path_potential(
        const Vehicle::VehicleTransformState& state,
        const std::vector<VehiclePathPoint>& path,
        const Vehicle::PathArcLengthTable& path_arc_lengths,
        float active_segment_min_s,
        float active_segment_max_s,
        float speed_acceleration,
        float steer_acceleration,
        float groove_radius
    ) const;

    float compute_slowdown_loss(
        const Vehicle::VehicleTransformState& state,
        const std::vector<VehiclePathPoint>& path,
        const Vehicle::PathArcLengthTable& path_arc_lengths,
        float active_segment_min_s,
        float active_segment_max_s,
        float speed_acceleration,
        float steer_acceleration
    ) const;

    float compute_cruise_speed_loss(const Vehicle::VehicleTransformState& state) const;

    PointProjection find_path_projection(
        const VehicleTransformState& state,
        const std::vector<VehiclePathPoint>& path,
        const PathArcLengthTable& path_arc_lengths,
        float min_s = 0.0f,
        float max_s = std::numeric_limits<float>::infinity()
    ) const;

private:
    VehicleTransformState m_vehicle_state;

    float m_max_acceleration; // max(p''(t)). Максимальное ускорение машины
    float m_max_steer_acceleration; // max(delta''(t)). Максимальное ускорение угла поворота колес
    float m_wheel_base; // L. Расстояние между передней и задней осями
    SimulationLossWeights m_loss_weights;
    SimulationFollowParams m_follow_params;
    PathPotentialParams m_path_potential_params;
    float m_path_potential_distance_exponent = 0.5f;

private:
    struct SimulationLossCoefficients {
        float position = 0.0f;
        float heading = 0.0f;
        float speed = 0.0f;
        float progress = 0.0f;
        float steering = 0.0f;
        float control = 0.0f;

        float total() const noexcept {
            return position + heading + speed + progress + steering + control;
        }
    };

    struct PathPotentialEvaluation {
        PointProjection projection;
        SimulationLossBreakdown components;

        float total() const noexcept {
            return components.total();
        }
    };

    void check_simulation_step(
        float speed_acceleration,
        float steer_acceleration,
        float dt
    ) const;

    static float angle_diff(float from, float to);
    static glm::vec2 forward_vector(const VehicleTransformState& state);

    float compute_reference_progress(
        float initial_path_speed,
        float time
    ) const;

    float compute_position_loss_coefficient(
        const VehicleTransformState& state,
        const PointProjection& target_projection
    ) const;

    float compute_heading_loss_coefficient(
        const VehicleTransformState& state,
        const PointProjection& target_projection
    ) const;

    float compute_speed_loss_coefficient(
        const VehicleTransformState& state,
        const PointProjection& projection,
        const PathArcLengthTable& path_arc_lengths,
        float total_polyline_length,
        float current_next_direction_change_s
    ) const;

    float compute_progress_loss_coefficient(
        const VehicleTransformState& state,
        const PointProjection& projection,
        const PointProjection& target_projection,
        float progress_reference_s
    ) const;

    float compute_steering_loss_coefficient(
        const VehicleTransformState& state,
        const PointProjection& target_projection
    ) const;

    float compute_control_loss_coefficient(
        const VehicleTransformState& state,
        float speed_acceleration,
        float steer_acceleration
    ) const;

    float compute_potential_control_regularization(
        const VehicleTransformState& state,
        float speed_acceleration,
        float steer_acceleration
    ) const;

    float compute_path_target_speed(
        const PointProjection& projection,
        float active_segment_end_s,
        bool slow_down_at_projection_max,
        float projection_max_target_speed_abs
    ) const;

    PathPotentialEvaluation evaluate_path_potential(
        const VehicleTransformState& state,
        const std::vector<VehiclePathPoint>& path,
        const PathArcLengthTable& path_arc_lengths,
        float active_segment_min_s,
        float active_segment_max_s,
        bool slow_down_at_projection_max,
        float projection_max_target_speed_abs,
        float speed_acceleration,
        float steer_acceleration
    ) const;

    SimulationLossCoefficients compute_simulation_loss_coefficients(
        const VehicleTransformState& state,
        const PointProjection& projection,
        const PointProjection& target_projection,
        const PathArcLengthTable& path_arc_lengths,
        float total_polyline_length,
        float progress_reference_s,
        float current_next_direction_change_s,
        float speed_acceleration,
        float steer_acceleration
    ) const;

    float compute_simulation_loss(
        VehicleTransformState& state,
        const std::vector<VehiclePathPoint>& path,
        const PathArcLengthTable& path_arc_lengths,
        float total_polyline_length,
        float speed_acceleration,
        float steer_acceleration,
        float simulation_time, 
        float dt = 0.05f,
        bool debug = true,
        float initial_projection_min_s = 0.0f,
        float initial_projection_max_s = std::numeric_limits<float>::infinity(),
        bool slow_down_at_projection_max = false,
        float projection_max_target_speed_abs = 0.0f,
        std::vector<glm::vec2>* trajectory = nullptr,
        SimulationControlCandidateDebug* candidate_debug = nullptr
    ) const;

    float compute_simulation_loss_test(
        VehicleTransformState& state,
        const std::vector<VehiclePathPoint>& path,
        const PathArcLengthTable& path_arc_lengths,
        float total_polyline_length,
        float speed_acceleration,
        float steer_acceleration,
        float max_simulation_time,
        float max_simulation_s,
        float dt = 0.05f,
        bool debug = true,
        float initial_projection_min_s = 0.0f,
        float initial_projection_max_s = std::numeric_limits<float>::infinity(),
        bool slow_down_at_projection_max = false,
        float projection_max_target_speed_abs = 0.0f,
        std::vector<glm::vec2>* trajectory = nullptr,
        SimulationControlCandidateDebug* candidate_debug = nullptr
    ) const;
};
