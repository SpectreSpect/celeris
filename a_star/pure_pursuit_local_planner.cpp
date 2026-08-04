#include "pure_pursuit_local_planner.h"

#include "path_intersection_detector.h"
#include "../autopilot/vehicle_command_sender.h"
#include "../vulkan_self/vulkan_submit_context.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
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

PurePursuitLocalPlanner::PurePursuitLocalPlanner(float step_dt_min, float step_dt_max)
    :   m_step_dt_min(step_dt_min),
        m_step_dt_max(step_dt_max) {}

void PurePursuitLocalPlanner::update_timestamp() {
    m_previous_timestamp = m_current_timestamp;
    m_current_timestamp = PurePursuitClock::now();
}

float PurePursuitLocalPlanner::calculate_delta_time() {
    float delta_time = m_step_dt_min;
    if (m_previous_timestamp.has_value() && m_current_timestamp.has_value()) {
        delta_time = std::chrono::duration<float>(*m_current_timestamp - *m_previous_timestamp).count();
        delta_time = std::clamp(delta_time, m_step_dt_min, m_step_dt_max);
    }

    return delta_time;
}

float PurePursuitLocalPlanner::calculate_command_delta_time() {
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

void PurePursuitLocalPlanner::predict_vehicle_state(PurePursuitVehicle& vehicle) {
    const float speed_acceleration = m_last_control_command.has_value()
        ? m_last_control_command->speed_acceleration
        : 0.0f;
    const float steering_angle_velocity = m_last_control_command.has_value()
        ? m_last_control_command->steering_angle_velocity
        : 0.0f;

    const float delta_time = calculate_delta_time();
    vehicle.simulate_vehicle(speed_acceleration, steering_angle_velocity, delta_time);
}

void PurePursuitLocalPlanner::predict_vehicle_state(VehicleBase& vehicle) {
    PurePursuitVehicle* pure_pursuit_vehicle = dynamic_cast<PurePursuitVehicle*>(&vehicle);
    logger().check(
        pure_pursuit_vehicle != nullptr,
        "PurePursuitLocalPlanner requires PurePursuitVehicle"
    );
    predict_vehicle_state(*pure_pursuit_vehicle);
}

const PurePursuitVehicle::PurePursuitStepResult&
PurePursuitLocalPlanner::last_step_result() const noexcept
{
    return m_last_step_result;
}

float PurePursuitLocalPlanner::path_progress_s() const noexcept {
    return m_path_progress_s;
}

float PurePursuitLocalPlanner::path_length() const noexcept {
    return m_path_length;
}

float PurePursuitLocalPlanner::path_window_min_s() const noexcept {
    return m_path_window_min_s;
}

float PurePursuitLocalPlanner::path_window_max_s() const noexcept {
    return m_path_window_max_s;
}

uint64_t PurePursuitLocalPlanner::path_generation() const noexcept {
    return m_path_generation;
}

const std::vector<VehiclePathPoint>& PurePursuitLocalPlanner::vehicle_path() const noexcept {
    return m_global_astar_path;
}

const VehicleBase::PathArcLengthTable&
PurePursuitLocalPlanner::vehicle_path_arc_lengths() const noexcept
{
    return m_global_astar_path_arc_lengths;
}

void PurePursuitLocalPlanner::reset_tracking()
{
    reset_tracking_state();
}

void PurePursuitLocalPlanner::reset_tracking_state()
{
    m_path_progress_s = 0.0f;
    m_path_window_min_s = 0.0f;
    m_path_window_max_s = 0.0f;
    m_path_progress_floor_s = 0.0f;
    m_has_path_progress = false;
    m_last_control_command.reset();
    m_last_step_result = PurePursuitVehicle::PurePursuitStepResult{};
    m_previous_command_timestamp.reset();
}

void PurePursuitLocalPlanner::set_vehicle_path(std::vector<VehiclePathPoint> path, bool reset_tracking)
{
    m_global_astar_path = std::move(path);
    m_global_astar_path_arc_lengths =
        Vehicle::build_path_arc_length_table(m_global_astar_path);
    m_path_length = Vehicle::polyline_length(m_global_astar_path_arc_lengths);

    if (reset_tracking)
        reset_tracking_state();
}

void PurePursuitLocalPlanner::set_astar_path(const std::vector<NonholonomicPos>& astar_path) {
    LOG_METHOD();

    std::vector<VehiclePathPoint> new_path = make_vehicle_path(astar_path);
    const bool path_changed = !same_path_signature(m_global_astar_path, new_path);
    set_vehicle_path(std::move(new_path), path_changed);
    if (path_changed)
        m_path_generation++;
}

void PurePursuitLocalPlanner::set_astar_path(
    const std::vector<NonholonomicPos>& astar_path,
    uint64_t generation)
{
    LOG_METHOD();

    if (m_path_generation == generation)
        return;

    m_path_generation = generation;
    set_vehicle_path(make_vehicle_path(astar_path), true);
}

VehicleCommand PurePursuitLocalPlanner::predict_vehicle_command(
    const VehicleBase& vehicle,
    PathIntersectionDetector& intersection_detector,
    VulkanSubmitContext& submit_context)
{
    const PurePursuitVehicle* pure_pursuit_vehicle =
        dynamic_cast<const PurePursuitVehicle*>(&vehicle);
    logger().check(
        pure_pursuit_vehicle != nullptr,
        "PurePursuitLocalPlanner requires PurePursuitVehicle"
    );
    return predict_vehicle_command(*pure_pursuit_vehicle, intersection_detector, submit_context);
}

VehicleCommand PurePursuitLocalPlanner::predict_vehicle_command(
    const PurePursuitVehicle& vehicle,
    PathIntersectionDetector& intersection_detector,
    VulkanSubmitContext& submit_context)
{
    if (m_global_astar_path.empty()) {
        m_last_control_command = PurePursuitVehicle::PurePursuitControlCommand{};
        m_last_step_result = PurePursuitVehicle::PurePursuitStepResult{};
        m_path_progress_s = 0.0f;
        m_path_length = 0.0f;
        m_path_window_min_s = 0.0f;
        m_path_window_max_s = 0.0f;
        m_path_progress_floor_s = 0.0f;
        return VehicleCommand{};
    }

    if (m_path_length <= Utils::eps) {
        m_last_control_command = PurePursuitVehicle::PurePursuitControlCommand{};
        m_last_step_result = PurePursuitVehicle::PurePursuitStepResult{};
        m_path_window_min_s = 0.0f;
        m_path_window_max_s = 0.0f;
        m_path_progress_floor_s = 0.0f;
        return VehicleCommand{};
    }

    const float command_delta_time = calculate_command_delta_time();
    const PurePursuitVehicle::PurePursuitParams& params = vehicle.params();
    float projection_min_s = m_path_progress_floor_s;
    float projection_max_s = std::numeric_limits<float>::infinity();
    if (m_has_path_progress) {
        projection_min_s = std::max(
            m_path_progress_floor_s,
            m_path_progress_s - params.projection_backtrack_window
        );
        projection_max_s = std::min(
            m_path_length,
            m_path_progress_s +
                params.projection_lookahead_base +
                std::abs(vehicle.state().speed) * command_delta_time
        );
    }

    Vehicle::PointProjection current_projection = Vehicle::find_polyline_projection(
        m_global_astar_path,
        m_global_astar_path_arc_lengths,
        vehicle.state().position,
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
        const bool close_to_switch =
            switch_distance_s >= -Utils::eps &&
            switch_distance_s <= std::max(0.0f, params.direction_switch_arrival_distance);
        const bool almost_stopped =
            std::abs(vehicle.state().speed) <=
            std::max(0.0f, params.direction_switch_arrival_speed);
        const bool close_to_path =
            current_projection.dist <=
            std::max(0.5f, params.slowdown_distance_from_path);

        if (close_to_switch && almost_stopped && close_to_path) {
            m_path_progress_floor_s = std::min(
                m_path_length,
                next_switch_s + kDirectionSwitchProgressEps
            );
            current_projection = Vehicle::find_polyline_projection(
                m_global_astar_path,
                m_global_astar_path_arc_lengths,
                vehicle.state().position,
                m_path_progress_floor_s,
                std::min(
                    m_path_length,
                    m_path_progress_floor_s + params.projection_lookahead_base
                )
            );
        }
    }

    m_path_progress_s = current_projection.s;
    m_has_path_progress = true;

    m_path_window_min_s = std::max(
        m_path_progress_floor_s,
        current_projection.s - params.projection_backtrack_window
    );
    m_path_window_max_s = std::min(
        m_path_length,
        current_projection.s +
            params.projection_lookahead_base +
            std::abs(vehicle.state().speed) * command_delta_time
    );

    Vehicle::PointProjection control_current_projection;
    Vehicle::PointProjection target_projection;
    float lookahead_distance = 0.0f;
    float target_steering_angle = 0.0f;
    float target_speed = 0.0f;
    const PurePursuitVehicle::PurePursuitControlCommand control = vehicle.compute_control(
        vehicle.state(),
        m_global_astar_path,
        m_global_astar_path_arc_lengths,
        m_path_window_min_s,
        m_path_window_max_s,
        &control_current_projection,
        &target_projection,
        &lookahead_distance,
        &target_steering_angle,
        &target_speed
    );
    m_last_control_command = control;

    m_last_step_result = PurePursuitVehicle::PurePursuitStepResult{
        .control_command = control,
        .state_after_step = vehicle.get_vehicle_simulation_step(
            vehicle.state(),
            control.speed_acceleration,
            control.steering_angle_velocity,
            command_delta_time
        ),
        .current_projection = control_current_projection,
        .target_projection = target_projection,
        .lookahead_distance = lookahead_distance,
        .target_steering_angle = target_steering_angle,
        .target_speed = target_speed
    };

    return VehicleCommand{
        .acceleration = control.speed_acceleration,
        .steering_angle_velocity = control.steering_angle_velocity
    };
}

VehicleCommand PurePursuitLocalPlanner::step(
    const VehicleBase& vehicle,
    PathIntersectionDetector& intersection_detector,
    VulkanSubmitContext& submit_context,
    const std::vector<NonholonomicPos>* astar_path)
{
    const PurePursuitVehicle* pure_pursuit_vehicle =
        dynamic_cast<const PurePursuitVehicle*>(&vehicle);
    logger().check(
        pure_pursuit_vehicle != nullptr,
        "PurePursuitLocalPlanner requires PurePursuitVehicle"
    );
    return step(*pure_pursuit_vehicle, intersection_detector, submit_context, astar_path);
}

VehicleCommand PurePursuitLocalPlanner::step(
    const PurePursuitVehicle& vehicle,
    PathIntersectionDetector& intersection_detector,
    VulkanSubmitContext& submit_context,
    const std::vector<NonholonomicPos>* astar_path)
{
    if (astar_path)
        set_astar_path(*astar_path);

    return predict_vehicle_command(vehicle, intersection_detector, submit_context);
}
