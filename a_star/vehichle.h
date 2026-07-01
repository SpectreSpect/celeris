#pragma once

#include <glm/glm.hpp>
#include <limits>
#include <vector>

#include "../vulkan_self/logger/logger_header.h"
#include "../renderer/transform.h"
#include "a_star_structures.h"

class Vehicle {
public:
    _XCLASS_NAME(Vehicle);

public:
    struct PointProjection {
        float s = 0.0f;
        float dist = 0.0f;
        glm::vec2 point = glm::vec2{0.0f};
        glm::vec2 tangent = glm::vec2{1.0f, 0.0f};
        float heading = 0.0f;
        float dir = 1.0f;
    };

public:
    struct VehicleTransformState {
        glm::vec2 m_position = glm::vec2{0.0f}; // p(t). Позиция машины
        float m_speed = 0.0f; // Скорость движения машины в направлении "взгляда"
        float m_speed_acceleration = 0.0f; // p''(t). Текущее продольное ускорение
        float m_heading = 0.0f; // theta(t). Направление кузова машины в плоскости XZ

        float m_steering_angle = 0.0f; // delta(t). Угол поворота передних колес относительно кузова
        float m_steering_angle_velocity = 0.0f; // delta'(t). Скорость изменения угла поворота колес
        float m_steering_angle_acceleration = 0.0f; // delta''(t). Ускорение угла поворота колес
    };

    struct SimulationControlSearchDesc {
        int speed_acceleration_samples = 7;
        int steer_acceleration_samples = 7;
        int max_results = 5;
        float simulation_time = 1.0f;
        float dt = 0.05f;
        bool debug = false;
    };

    struct VehicleControlCommand {
        float speed_acceleration = 0.0f;
        float steer_acceleration = 0.0f;
    };

    struct SimulationControlCandidate {
        VehicleControlCommand control_command;
        float loss = std::numeric_limits<float>::infinity();
        VehicleTransformState predicted_state;
        std::vector<glm::vec2> trajectory;
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
    ~Vehicle() noexcept = default;

    Vehicle(const Vehicle&) = delete;
    Vehicle& operator=(const Vehicle&) = delete;

    Vehicle(Vehicle&&) noexcept = default;
    Vehicle& operator=(Vehicle&&) noexcept = default;

    VehicleTransformState& state() noexcept;
    const VehicleTransformState& state() const noexcept;

    void vehicle_simulation_step(
        VehicleTransformState& state,
        float speed_acceleration,
        float steer_acceleration,
        float dt = 0.05f,
        bool debug = true
    ) const;

    VehicleTransformState get_vehicle_simulation_step(
        const VehicleTransformState& state,
        float speed_acceleration,
        float steer_acceleration,
        float dt = 0.05f,
        bool debug = true
    ) const;

    void vehicle_simulation_step(
        float speed_acceleration,
        float steer_acceleration,
        float dt = 0.05f,
        bool debug = true
    );

    VehicleTransformState get_vehicle_simulation_step(
        float speed_acceleration,
        float steer_acceleration,
        float dt = 0.05f,
        bool debug = true
    );

    void simulate_vehicle(
        float speed_acceleration,
        float steer_acceleration,
        float simulation_time, 
        float dt = 0.05f,
        bool debug = true
    );

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

private:
    VehicleTransformState m_vehicle_state;

    float m_max_acceleration; // max(p''(t)). Максимальное ускорение машины
    float m_max_steer_acceleration; // max(delta''(t)). Максимальное ускорение угла поворота колес
    float m_wheel_base; // L. Расстояние между передней и задней осями

private:
    struct SimulationLossCoefficients {
        float position = 0.0f;
        float heading = 0.0f;
        float speed = 0.0f;
        float progress = 0.0f;
        float control = 0.0f;

        float total() const noexcept {
            return position + heading + speed + progress + control;
        }
    };

    void check_simulation_step(
        float speed_acceleration,
        float steer_acceleration,
        float dt
    ) const;

    void simulate_vehicle(
        VehicleTransformState& state,
        float speed_acceleration,
        float steer_acceleration,
        float simulation_time, 
        float dt = 0.05f,
        bool debug = true
    ) const;

    static PointProjection find_polyline_projection(
        const std::vector<VehiclePathPoint>& path,
        glm::vec2 point,
        float min_s = 0.0f,
        float max_s = std::numeric_limits<float>::infinity()
    );

    static float polyline_length(const std::vector<VehiclePathPoint>& path);
    static float angle_diff(float from, float to);
    static glm::vec2 forward_vector(const VehicleTransformState& state);

    static float compute_position_loss_coefficient(
        const VehicleTransformState& state,
        const PointProjection& projection
    );

    static float compute_heading_loss_coefficient(
        const VehicleTransformState& state,
        const PointProjection& projection
    );

    float compute_speed_loss_coefficient(
        const VehicleTransformState& state,
        const PointProjection& projection,
        float total_polyline_length
    ) const;

    static float compute_progress_loss_coefficient(
        const VehicleTransformState& state,
        const PointProjection& projection
    );

    float compute_control_loss_coefficient(
        const VehicleTransformState& state,
        float speed_acceleration,
        float steer_acceleration
    ) const;

    SimulationLossCoefficients compute_simulation_loss_coefficients(
        const VehicleTransformState& state,
        const PointProjection& projection,
        float total_polyline_length,
        float speed_acceleration,
        float steer_acceleration
    ) const;

    float compute_simulation_loss(
        VehicleTransformState& state,
        const std::vector<VehiclePathPoint>& path,
        float speed_acceleration,
        float steer_acceleration,
        float simulation_time, 
        float dt = 0.05f,
        bool debug = true,
        std::vector<glm::vec2>* trajectory = nullptr
    ) const;
};
