#include "local_planner.h"

#include "vehichle.h"
#include "path_intersection_detector.h"
#include "../vulkan_self/vulkan_submit_context.h"
#include "../autopilot/vehicle_command_sender.h"

LocalPlanner::LocalPlanner(float step_dt_min, float step_dt_max)
    :   m_step_dt_min(step_dt_min),
        m_step_dt_max(step_dt_max) {}

void LocalPlanner::set_astar_path(const std::vector<NonholonomicPos>& astar_path) {
    LOG_METHOD();
    m_global_astar_path = make_vehicle_path(astar_path);
}

void LocalPlanner::update_timestamp() {
    LOG_METHOD();

    m_previous_timestamp = m_current_timestamp;
    m_current_timestamp = Clock::now();
}

float LocalPlanner::calculate_delta_time() {
    LOG_METHOD();

    float delta_time = m_step_dt_min;
    if (m_previous_timestamp.has_value() && m_current_timestamp.has_value()) {
        delta_time = std::chrono::duration<float>(*m_current_timestamp - *m_previous_timestamp).count();
        delta_time = std::clamp(delta_time, m_step_dt_min, m_step_dt_max);
    }

    return delta_time;
}

void LocalPlanner::predict_vehicle_state(Vehicle& vehicle) {
    LOG_METHOD();

    const float speed_acceleration = 
        m_last_applied_command.has_value() ? m_last_applied_command->speed_acceleration : 0.0f;

    if (m_last_applied_command.has_value())
        vehicle.state().m_steering_angle_velocity = m_last_applied_command->steering_angle_velocity;

    const float delta_time = calculate_delta_time();

    /*
        Симулируем динамику на время delta_time - так будет 
        немного точнее, если delta_time будет большим.
    */
    vehicle.simulate_vehicle(speed_acceleration, 0.0f, delta_time);
}

const std::vector<Vehicle::SimulationControlCandidate>&
LocalPlanner::last_simulation_candidates() const noexcept
{
    return m_last_simulation_candidates;
}

VehicleCommand LocalPlanner::predict_vehicle_command(
    const Vehicle& vehicle,
    PathIntersectionDetector& intersection_detector,
    VulkanSubmitContext& submit_context)
{
    LOG_METHOD();

    m_last_simulation_candidates = vehicle.find_best_simulation_controls(m_global_astar_path);
    
    const Vehicle::VehicleControlCommand control
        = m_last_simulation_candidates.empty()
        ? Vehicle::VehicleControlCommand()
        : m_last_simulation_candidates.front().control_command;

    last_control_command = control;

    const float steering_angle_velocity_target = 
        vehicle.state().m_steering_angle_velocity + 0.5f * control.steer_acceleration * calculate_delta_time();

    m_last_applied_command = AppliedVehicleCommand{
        .speed_acceleration = control.speed_acceleration,
        .steering_angle_velocity = steering_angle_velocity_target
    };

    return VehicleCommand{
        .acceleration = control.speed_acceleration,
        .steering_angle_velocity = steering_angle_velocity_target
    };
}
