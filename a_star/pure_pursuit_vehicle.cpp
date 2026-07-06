#include "pure_pursuit_vehicle.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../vulkan_self/utils.h"

namespace {
    constexpr float kMaxSteeringAngle = 0.7f;
    constexpr float kPi = 3.14159265358979323846f;

    float angle_diff(float from, float to)
    {
        constexpr float two_pi = 6.2831853071795864769f;

        float diff = std::fmod(to - from, two_pi);
        if (diff <= -kPi) diff += two_pi;
        if (diff > kPi) diff -= two_pi;
        return diff;
    }

    glm::vec2 forward_vector(const Vehicle::VehicleTransformState& state)
    {
        return glm::vec2{std::cos(state.m_heading), std::sin(state.m_heading)};
    }
}

PurePursuitVehicle::PurePursuitVehicle(
    const VehicleTransformState& initial_state,
    float max_acceleration,
    float max_steering_angle_velocity,
    float wheel_base)
    :   m_vehicle_state(initial_state),
        m_max_acceleration(max_acceleration),
        m_max_steering_angle_velocity(max_steering_angle_velocity),
        m_wheel_base(wheel_base)
{
    logger().check(m_wheel_base > Utils::eps, "wheel_base must be greater than zero");
}

PurePursuitVehicle::PurePursuitVehicle(
    float max_acceleration,
    float max_steering_angle_velocity,
    float wheel_base)
    :   PurePursuitVehicle(
            VehicleTransformState{},
            max_acceleration,
            max_steering_angle_velocity,
            wheel_base
        ) {}

PurePursuitVehicle::VehicleTransformState& PurePursuitVehicle::state() noexcept {
    return m_vehicle_state;
}

const PurePursuitVehicle::VehicleTransformState& PurePursuitVehicle::state() const noexcept {
    return m_vehicle_state;
}

PurePursuitVehicle::PurePursuitParams& PurePursuitVehicle::params() noexcept {
    return m_params;
}

const PurePursuitVehicle::PurePursuitParams& PurePursuitVehicle::params() const noexcept {
    return m_params;
}

void PurePursuitVehicle::reset_params() noexcept {
    m_params = PurePursuitParams{};
}

void PurePursuitVehicle::check_simulation_step(
    float speed_acceleration,
    float steering_angle_velocity,
    float dt) const
{
    logger().check(dt > 0.0f, "dt must be greater than zero");
    logger().check(
        std::abs(speed_acceleration) <= m_max_acceleration,
        "abs(speed_acceleration) must be less than m_max_acceleration"
    );
    logger().check(
        std::abs(steering_angle_velocity) <= m_max_steering_angle_velocity,
        "abs(steering_angle_velocity) must be less than m_max_steering_angle_velocity"
    );
}

void PurePursuitVehicle::vehicle_simulation_step(
    VehicleTransformState& state,
    float speed_acceleration,
    float steering_angle_velocity,
    float dt,
    bool debug) const
{
    if (debug) {
        check_simulation_step(speed_acceleration, steering_angle_velocity, dt);
    }

    const float speed0 = state.m_speed;
    const float heading0 = state.m_heading;
    const float steering_angle0 = std::clamp(
        state.m_steering_angle,
        -kMaxSteeringAngle,
        kMaxSteeringAngle
    );
    float steering_angle_velocity0 = steering_angle_velocity;
    if ((steering_angle0 <= -kMaxSteeringAngle + Utils::eps && steering_angle_velocity0 < 0.0f) ||
        (steering_angle0 >= kMaxSteeringAngle - Utils::eps && steering_angle_velocity0 > 0.0f))
    {
        steering_angle_velocity0 = 0.0f;
    }

    const glm::vec2 velocity0 = forward_vector(state) * speed0;
    const float heading_velocity0 = speed0 * std::tan(steering_angle0) / m_wheel_base;

    const float speed1 = speed0 + speed_acceleration * dt;
    const float steering_angle1 = std::clamp(
        steering_angle0 + steering_angle_velocity0 * dt,
        -kMaxSteeringAngle,
        kMaxSteeringAngle
    );
    if ((steering_angle1 <= -kMaxSteeringAngle + Utils::eps && steering_angle_velocity0 < 0.0f) ||
        (steering_angle1 >= kMaxSteeringAngle - Utils::eps && steering_angle_velocity0 > 0.0f))
    {
        steering_angle_velocity0 = 0.0f;
    }

    const float heading_velocity1 = speed1 * std::tan(steering_angle1) / m_wheel_base;
    const float heading1 = heading0 + 0.5f * (heading_velocity0 + heading_velocity1) * dt;
    const glm::vec2 velocity1 = glm::vec2{std::cos(heading1), std::sin(heading1)} * speed1;

    state.m_position += 0.5f * (velocity0 + velocity1) * dt;
    state.m_speed = speed1;
    state.m_speed_acceleration = speed_acceleration;
    state.m_heading = heading1;
    state.m_steering_angle = steering_angle1;
    state.m_steering_angle_velocity = steering_angle_velocity0;
    state.m_steering_angle_acceleration = 0.0f;
}

PurePursuitVehicle::VehicleTransformState PurePursuitVehicle::get_vehicle_simulation_step(
    const VehicleTransformState& state,
    float speed_acceleration,
    float steering_angle_velocity,
    float dt,
    bool debug) const
{
    VehicleTransformState result = state;
    vehicle_simulation_step(result, speed_acceleration, steering_angle_velocity, dt, debug);
    return result;
}

void PurePursuitVehicle::vehicle_simulation_step(
    float speed_acceleration,
    float steering_angle_velocity,
    float dt,
    bool debug)
{
    vehicle_simulation_step(m_vehicle_state, speed_acceleration, steering_angle_velocity, dt, debug);
}

PurePursuitVehicle::VehicleTransformState PurePursuitVehicle::get_vehicle_simulation_step(
    float speed_acceleration,
    float steering_angle_velocity,
    float dt,
    bool debug)
{
    return get_vehicle_simulation_step(m_vehicle_state, speed_acceleration, steering_angle_velocity, dt, debug);
}

void PurePursuitVehicle::simulate_vehicle(
    float speed_acceleration,
    float steering_angle_velocity,
    float simulation_time,
    float dt,
    bool debug)
{
    simulate_vehicle(m_vehicle_state, speed_acceleration, steering_angle_velocity, simulation_time, dt, debug);
}

void PurePursuitVehicle::simulate_vehicle(
    VehicleTransformState& state,
    float speed_acceleration,
    float steering_angle_velocity,
    float simulation_time,
    float dt,
    bool debug) const
{
    if (debug) {
        logger().check(simulation_time > 0.0f, "simulation_time must be greater than zero");
        check_simulation_step(speed_acceleration, steering_angle_velocity, dt);
    }

    for (float time = 0.0f; time < simulation_time;) {
        const float step_dt = std::min(dt, simulation_time - time);
        vehicle_simulation_step(state, speed_acceleration, steering_angle_velocity, step_dt, false);
        time += step_dt;
    }
}

PurePursuitVehicle::PurePursuitControlCommand PurePursuitVehicle::compute_control(
    const VehicleTransformState& state,
    const std::vector<VehiclePathPoint>& path,
    const PathArcLengthTable& path_arc_lengths,
    float projection_min_s,
    float projection_max_s,
    PointProjection* current_projection_out,
    PointProjection* target_projection_out,
    float* lookahead_distance_out,
    float* target_steering_angle_out,
    float* target_speed_out) const
{
    logger().check(!path.empty(), "path must contain at least one point");

    const float total_length = Vehicle::polyline_length(path_arc_lengths);
    if (total_length <= Utils::eps) {
        return PurePursuitControlCommand{};
    }

    PointProjection current_projection = Vehicle::find_polyline_projection(
        path,
        path_arc_lengths,
        state.m_position,
        projection_min_s,
        projection_max_s
    );

    const float lookahead_distance = std::clamp(
        m_params.lookahead_distance + std::abs(state.m_speed) * m_params.lookahead_speed_gain,
        std::max(0.0f, m_params.min_lookahead_distance),
        std::max(m_params.min_lookahead_distance, m_params.max_lookahead_distance)
    );
    PointProjection target_projection = Vehicle::sample_polyline_at_s(
        path,
        path_arc_lengths,
        current_projection.s + lookahead_distance
    );

    const float next_switch_s = Vehicle::next_direction_change_s(path, path_arc_lengths, current_projection.s);
    const bool final_direction_segment = !std::isfinite(next_switch_s);
    float remaining_s = std::max(0.0f, total_length - current_projection.s);
    if (!final_direction_segment) {
        remaining_s = std::max(0.0f, next_switch_s - current_projection.s);
    }

    const glm::vec2 to_target = target_projection.point - state.m_position;
    const float target_distance = glm::length(to_target);
    float target_steering_angle = 0.0f;
    if (target_distance > Utils::eps) {
        const float target_heading = std::atan2(to_target.y, to_target.x);
        const float motion_heading = target_projection.dir < 0.0f
            ? state.m_heading + kPi
            : state.m_heading;
        const float heading_error = angle_diff(motion_heading, target_heading);
        const float steering_lookahead_distance = std::max(
            target_distance,
            std::max(0.0f, m_params.min_steering_lookahead_distance)
        );
        target_steering_angle = std::clamp(
            std::atan2(2.0f * m_wheel_base * std::sin(heading_error), steering_lookahead_distance) *
                target_projection.dir,
            -kMaxSteeringAngle,
            kMaxSteeringAngle
        );

        if (final_direction_segment) {
            if (m_params.goal_steering_release_distance > Utils::eps) {
                target_steering_angle *= std::clamp(
                    remaining_s / m_params.goal_steering_release_distance,
                    0.0f,
                    1.0f
                );
            }

            if (remaining_s <= std::max(0.0f, m_params.goal_steering_release_distance) &&
                std::abs(state.m_speed) <= std::max(0.0f, m_params.goal_steering_release_speed))
            {
                target_steering_angle = 0.0f;
            }
        }
    }

    const float braking_speed = std::sqrt(std::max(0.0f, 2.0f * m_max_acceleration * remaining_s));
    float target_speed_abs = std::min(std::max(0.0f, m_params.cruise_speed), braking_speed);
    if (std::isfinite(next_switch_s)) {
        target_speed_abs = std::max(
            target_speed_abs,
            std::min(
                std::max(0.0f, m_params.cruise_speed),
                std::max(0.0f, m_params.direction_switch_approach_speed)
            )
        );
    }

    if (m_params.slowdown_distance_from_path > Utils::eps) {
        const float off_path_speed_factor = std::clamp(
            1.0f - current_projection.dist / m_params.slowdown_distance_from_path,
            std::clamp(m_params.min_off_path_speed_factor, 0.0f, 1.0f),
            1.0f
        );
        target_speed_abs *= off_path_speed_factor;
    }

    const float target_speed = target_speed_abs * current_projection.dir;
    const float speed_acceleration = std::clamp(
        (target_speed - state.m_speed) * m_params.speed_p_gain,
        -m_max_acceleration,
        m_max_acceleration
    );
    const float steering_angle_velocity = std::clamp(
        angle_diff(state.m_steering_angle, target_steering_angle) * m_params.steering_p_gain,
        -m_max_steering_angle_velocity,
        m_max_steering_angle_velocity
    );

    if (current_projection_out) *current_projection_out = current_projection;
    if (target_projection_out) *target_projection_out = target_projection;
    if (lookahead_distance_out) *lookahead_distance_out = lookahead_distance;
    if (target_steering_angle_out) *target_steering_angle_out = target_steering_angle;
    if (target_speed_out) *target_speed_out = target_speed;

    return PurePursuitControlCommand{
        .speed_acceleration = speed_acceleration,
        .steering_angle_velocity = steering_angle_velocity
    };
}

PurePursuitVehicle::PurePursuitStepResult PurePursuitVehicle::follow_polyline_step(
    VehicleTransformState& state,
    const std::vector<glm::vec2>& polyline,
    float dt) const
{
    return follow_polyline_step(state, make_forward_vehicle_path(polyline), dt);
}

PurePursuitVehicle::PurePursuitStepResult PurePursuitVehicle::follow_polyline_step(
    VehicleTransformState& state,
    const std::vector<VehiclePathPoint>& path,
    float dt) const
{
    return follow_polyline_step(
        state,
        path,
        Vehicle::build_path_arc_length_table(path),
        0.0f,
        std::numeric_limits<float>::infinity(),
        dt
    );
}

PurePursuitVehicle::PurePursuitStepResult PurePursuitVehicle::follow_polyline_step(
    VehicleTransformState& state,
    const std::vector<VehiclePathPoint>& path,
    const PathArcLengthTable& path_arc_lengths,
    float projection_min_s,
    float projection_max_s,
    float dt) const
{
    PointProjection current_projection;
    PointProjection target_projection;
    float lookahead_distance = 0.0f;
    float target_steering_angle = 0.0f;
    float target_speed = 0.0f;
    const PurePursuitControlCommand control = compute_control(
        state,
        path,
        path_arc_lengths,
        projection_min_s,
        projection_max_s,
        &current_projection,
        &target_projection,
        &lookahead_distance,
        &target_steering_angle,
        &target_speed
    );

    vehicle_simulation_step(
        state,
        control.speed_acceleration,
        control.steering_angle_velocity,
        dt
    );

    return PurePursuitStepResult{
        .control_command = control,
        .state_after_step = state,
        .current_projection = current_projection,
        .target_projection = target_projection,
        .lookahead_distance = lookahead_distance,
        .target_steering_angle = target_steering_angle,
        .target_speed = target_speed
    };
}

PurePursuitVehicle::PurePursuitStepResult PurePursuitVehicle::follow_polyline_step(
    const std::vector<glm::vec2>& polyline,
    float dt)
{
    return follow_polyline_step(m_vehicle_state, make_forward_vehicle_path(polyline), dt);
}

PurePursuitVehicle::PurePursuitStepResult PurePursuitVehicle::follow_polyline_step(
    const std::vector<VehiclePathPoint>& path,
    float dt)
{
    return follow_polyline_step(m_vehicle_state, path, dt);
}
