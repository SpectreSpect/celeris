#include "local_planner.h"

#include "vehichle.h"
#include "path_intersection_detector.h"
#include "../vulkan_self/vulkan_submit_context.h"
#include "../autopilot/vehicle_command_sender.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
    constexpr float kMaxSteeringAngle = 0.7f;
    constexpr float kPathChangePositionEps2 = 0.05f * 0.05f;
    constexpr float kPathChangeAngleEps = 0.05f;
    constexpr float kDirectionSwitchProgressEps = 1e-3f;

    float angle_diff(float from, float to)
    {
        constexpr float two_pi = 6.2831853071795864769f;
        constexpr float pi = 3.14159265358979323846f;

        float diff = std::fmod(to - from, two_pi);
        if (diff <= -pi) diff += two_pi;
        if (diff > pi) diff -= two_pi;
        return diff;
    }

    bool same_path_point(const VehiclePathPoint& a, const VehiclePathPoint& b)
    {
        const glm::vec2 position_diff = a.position - b.position;
        return
            glm::dot(position_diff, position_diff) <= kPathChangePositionEps2 &&
            std::abs(angle_diff(a.heading, b.heading)) <= kPathChangeAngleEps &&
            a.dir == b.dir;
    }

    bool same_path_signature(
        const std::vector<VehiclePathPoint>& a,
        const std::vector<VehiclePathPoint>& b)
    {
        if (a.size() != b.size())
            return false;
        if (a.empty())
            return true;

        const size_t middle = a.size() / 2;
        return
            same_path_point(a.front(), b.front()) &&
            same_path_point(a[middle], b[middle]) &&
            same_path_point(a.back(), b.back());
    }
}

LocalPlanner::LocalPlanner(float step_dt_min, float step_dt_max)
    :   m_step_dt_min(step_dt_min),
        m_step_dt_max(step_dt_max) {}

void LocalPlanner::update_timestamp() {
    m_previous_timestamp = m_current_timestamp;
    m_current_timestamp = Clock::now();
}

float LocalPlanner::calculate_delta_time() {
    float delta_time = m_step_dt_min;
    if (m_previous_timestamp.has_value() && m_current_timestamp.has_value()) {
        delta_time = std::chrono::duration<float>(*m_current_timestamp - *m_previous_timestamp).count();
        delta_time = std::clamp(delta_time, m_step_dt_min, m_step_dt_max);
    }

    return delta_time;
}

float LocalPlanner::calculate_command_delta_time() {
    float delta_time = calculate_delta_time();
    if (m_previous_command_timestamp.has_value() && m_current_timestamp.has_value()) {
        delta_time = std::chrono::duration<float>(
            *m_current_timestamp - *m_previous_command_timestamp
        ).count();
        delta_time = std::clamp(delta_time, m_step_dt_min, m_step_dt_max);
    }

    if (m_current_timestamp.has_value())
        m_previous_command_timestamp = m_current_timestamp;

    return delta_time;
}

void LocalPlanner::predict_vehicle_state(Vehicle& vehicle) {
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

float LocalPlanner::path_progress_s() const noexcept {
    return m_path_progress_s;
}

float LocalPlanner::path_length() const noexcept {
    return m_path_length;
}

float LocalPlanner::path_window_min_s() const noexcept {
    return m_path_window_min_s;
}

float LocalPlanner::path_window_max_s() const noexcept {
    return m_path_window_max_s;
}

uint64_t LocalPlanner::path_generation() const noexcept {
    return m_path_generation;
}

void LocalPlanner::reset_tracking_state()
{
    m_path_progress_s = 0.0f;
    m_path_window_min_s = 0.0f;
    m_path_window_max_s = 0.0f;
    m_path_progress_floor_s = 0.0f;
    m_has_path_progress = false;
    m_last_simulation_candidates.clear();
    last_control_command.reset();
    m_last_applied_command.reset();
    m_previous_command_timestamp.reset();
}

void LocalPlanner::set_vehicle_path(std::vector<VehiclePathPoint> path, bool reset_tracking)
{
    m_global_astar_path = std::move(path);
    m_global_astar_path_arc_lengths =
        Vehicle::build_path_arc_length_table(m_global_astar_path);
    m_path_length = Vehicle::polyline_length(m_global_astar_path_arc_lengths);

    if (reset_tracking)
        reset_tracking_state();
}

void LocalPlanner::set_astar_path(const std::vector<NonholonomicPos>& astar_path) {
    LOG_METHOD();

    std::vector<VehiclePathPoint> new_path = make_vehicle_path(astar_path);
    const bool path_changed = !same_path_signature(m_global_astar_path, new_path);
    set_vehicle_path(std::move(new_path), path_changed);
    if (path_changed)
        m_path_generation++;
}

void LocalPlanner::set_astar_path(
    const std::vector<NonholonomicPos>& astar_path,
    uint64_t generation)
{
    LOG_METHOD();

    if (m_path_generation == generation)
        return;

    m_path_generation = generation;
    set_vehicle_path(make_vehicle_path(astar_path), true);
}

VehicleCommand LocalPlanner::predict_vehicle_command(
    const Vehicle& vehicle,
    PathIntersectionDetector& intersection_detector,
    VulkanSubmitContext& submit_context)
{
    if (m_global_astar_path.empty()) {
        m_last_simulation_candidates.clear();
        last_control_command = Vehicle::VehicleControlCommand{};
        m_last_applied_command = AppliedVehicleCommand{};
        m_path_progress_s = 0.0f;
        m_path_length = 0.0f;
        m_path_window_min_s = 0.0f;
        m_path_window_max_s = 0.0f;
        m_path_progress_floor_s = 0.0f;
        return VehicleCommand{};
    }

    if (m_path_length <= Utils::eps) {
        m_last_simulation_candidates.clear();
        last_control_command = Vehicle::VehicleControlCommand{};
        m_last_applied_command = AppliedVehicleCommand{};
        m_path_window_min_s = 0.0f;
        m_path_window_max_s = 0.0f;
        m_path_progress_floor_s = 0.0f;
        return VehicleCommand{};
    }

    const float command_delta_time = calculate_command_delta_time();
    const Vehicle::SimulationFollowParams& follow_params = vehicle.follow_params();
    float projection_min_s = m_path_progress_floor_s;
    float projection_max_s = std::numeric_limits<float>::infinity();
    if (m_has_path_progress) {
        projection_min_s = std::max(
            m_path_progress_floor_s,
            m_path_progress_s - follow_params.projection_backtrack_window
        );
        projection_max_s = std::min(
            m_path_length,
            m_path_progress_s +
                follow_params.projection_lookahead_base +
                std::abs(vehicle.state().m_speed) * command_delta_time
        );
    }

    Vehicle::PointProjection current_projection = Vehicle::find_polyline_projection(
        m_global_astar_path,
        m_global_astar_path_arc_lengths,
        vehicle.state().m_position,
        projection_min_s,
        projection_max_s
    );

    const float next_switch_s = Vehicle::next_direction_change_s(
        m_global_astar_path,
        m_global_astar_path_arc_lengths,
        current_projection.s
    );
    if (std::isfinite(next_switch_s)) {
        const float switch_distance_s = next_switch_s - current_projection.s;
        const float arrival_distance =
            std::max(0.0f, follow_params.direction_switch_arrival_distance);
        const float arrival_speed =
            std::max(
                std::max(0.0f, follow_params.direction_switch_arrival_speed),
                std::max(0.0f, follow_params.direction_switch_approach_speed)
            );
        const bool close_to_switch =
            switch_distance_s >= -Utils::eps &&
            switch_distance_s <= arrival_distance;
        const bool almost_stopped = std::abs(vehicle.state().m_speed) <= arrival_speed;
        const bool close_to_path =
            current_projection.dist <=
            std::max(0.5f, follow_params.slowdown_distance_from_path);

        if (close_to_switch && almost_stopped && close_to_path) {
            m_path_progress_floor_s = std::min(
                m_path_length,
                next_switch_s + kDirectionSwitchProgressEps
            );
            current_projection = Vehicle::find_polyline_projection(
                m_global_astar_path,
                m_global_astar_path_arc_lengths,
                vehicle.state().m_position,
                m_path_progress_floor_s,
                std::min(
                    m_path_length,
                    m_path_progress_floor_s + follow_params.projection_lookahead_base
                )
            );
        }
    }

    m_path_progress_s = current_projection.s;
    m_has_path_progress = true;

    const float candidate_projection_min_s = std::max(
        m_path_progress_floor_s,
        current_projection.s - follow_params.projection_backtrack_window
    );
    const float candidate_projection_max_s = std::min(
        m_path_length,
        current_projection.s +
            follow_params.projection_lookahead_base +
            std::abs(vehicle.state().m_speed) * command_delta_time
    );
    m_path_window_min_s = candidate_projection_min_s;
    m_path_window_max_s = candidate_projection_max_s;

    Vehicle::SimulationControlSearchDesc search_desc;
    search_desc.max_results =
        search_desc.speed_acceleration_samples *
        search_desc.steer_acceleration_samples;
    search_desc.initial_projection_min_s = candidate_projection_min_s;
    search_desc.initial_projection_max_s = candidate_projection_max_s;

    m_last_simulation_candidates = vehicle.find_best_simulation_controls(
        m_global_astar_path,
        m_global_astar_path_arc_lengths,
        search_desc
    );
    
    const Vehicle::VehicleControlCommand control
        = m_last_simulation_candidates.empty()
        ? Vehicle::VehicleControlCommand()
        : m_last_simulation_candidates.front().control_command;

    last_control_command = control;

    float effective_steering_angle_velocity = vehicle.state().m_steering_angle_velocity;
    if ((vehicle.state().m_steering_angle <= -kMaxSteeringAngle + Utils::eps &&
         effective_steering_angle_velocity < 0.0f) ||
        (vehicle.state().m_steering_angle >= kMaxSteeringAngle - Utils::eps &&
         effective_steering_angle_velocity > 0.0f))
    {
        effective_steering_angle_velocity = 0.0f;
    }
    float steering_angle_velocity_target =
        effective_steering_angle_velocity + 0.5f * control.steer_acceleration * command_delta_time;
    if ((vehicle.state().m_steering_angle <= -kMaxSteeringAngle + Utils::eps &&
         steering_angle_velocity_target < 0.0f) ||
        (vehicle.state().m_steering_angle >= kMaxSteeringAngle - Utils::eps &&
         steering_angle_velocity_target > 0.0f))
    {
        steering_angle_velocity_target = 0.0f;
    }

    m_last_applied_command = AppliedVehicleCommand{
        .speed_acceleration = control.speed_acceleration,
        .steering_angle_velocity = steering_angle_velocity_target
    };

    return VehicleCommand{
        .acceleration = control.speed_acceleration,
        .steering_angle_velocity = steering_angle_velocity_target
    };
}

VehicleCommand LocalPlanner::step(
    const Vehicle& vehicle,
    PathIntersectionDetector& intersection_detector,
    VulkanSubmitContext& submit_context,
    const std::vector<NonholonomicPos>* astar_path)
{
    if (astar_path)
        set_astar_path(*astar_path);

    return predict_vehicle_command(vehicle, intersection_detector, submit_context);
}
