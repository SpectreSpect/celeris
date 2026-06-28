#include "vehichle.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <utility>

namespace {
    constexpr float kEps = 1e-6f;

    constexpr float kPositionLossWeight = 4.0f;
    constexpr float kHeadingLossWeight = 0.6f;
    constexpr float kSpeedLossWeight = 0.25f;
    constexpr float kBackwardProgressLossWeight = 3.0f;
    constexpr float kControlLossWeight = 0.04f;
    constexpr float kSteeringRateLossWeight = 0.2f;

    constexpr float kCruiseSpeed = 3.0f;
    constexpr float kSlowdownDistanceFromPath = 2.5f;
    constexpr float kMinOffPathSpeedFactor = 0.25f;
    constexpr float kProjectionBacktrackWindow = 0.75f;
    constexpr float kProjectionLookaheadBase = 6.0f;

    float sample_symmetric_range(float max_abs_value, int sample_id, int sample_count) {
        if (sample_count <= 1 || max_abs_value <= kEps) {
            return 0.0f;
        }

        const float t = static_cast<float>(sample_id) / static_cast<float>(sample_count - 1);
        return -max_abs_value + 2.0f * max_abs_value * t;
    }
}

Vehicle::Vehicle(const VehicleTransformState& initial_state, float max_acceleration, float max_steer_acceleration)
    :   m_vehicle_state(initial_state),
        m_max_acceleration(max_acceleration),
        m_max_steer_acceleration(max_steer_acceleration) {}

Vehicle::Vehicle(float max_acceleration, float max_steer_acceleration)
    :   Vehicle(
            VehicleTransformState{}, 
            max_acceleration, 
            max_steer_acceleration
        ) {}

void Vehicle::vehicle_simulation_step(
    float speed_acceleration,
    float steer_acceleration,
    float dt,
    bool debug)
{
    vehicle_simulation_step(m_vehicle_state, speed_acceleration, steer_acceleration, dt, debug);
}

void Vehicle::simulate_vehicle(
    float speed_acceleration,
    float steer_acceleration,
    float simulation_time, 
    float dt,
    bool debug) 
{
    simulate_vehicle(m_vehicle_state, speed_acceleration, steer_acceleration, simulation_time, dt, debug);
}

Vehicle::PolylineFollowStepResult Vehicle::follow_polyline_step(
    VehicleTransformState& state,
    const std::vector<glm::vec2>& polyline) const
{
    return follow_polyline_step(state, polyline, SimulationControlSearchDesc{});
}

Vehicle::PolylineFollowStepResult Vehicle::follow_polyline_step(
    VehicleTransformState& state,
    const std::vector<glm::vec2>& polyline,
    const SimulationControlSearchDesc& desc) const
{
    LOG_METHOD();

    std::vector<SimulationControlCandidate> candidates = find_best_simulation_controls(state, polyline, desc);
    logger().check(!candidates.empty(), "no simulation control candidates found");

    SimulationControlCandidate selected_control = candidates.front();
    vehicle_simulation_step(
        state,
        selected_control.speed_acceleration,
        selected_control.steer_acceleration,
        desc.dt,
        desc.debug
    );

    return PolylineFollowStepResult{
        .selected_control = selected_control,
        .state_after_step = state
    };
}

Vehicle::PolylineFollowStepResult Vehicle::follow_polyline_step(
    const std::vector<glm::vec2>& polyline)
{
    return follow_polyline_step(m_vehicle_state, polyline, SimulationControlSearchDesc{});
}

Vehicle::PolylineFollowStepResult Vehicle::follow_polyline_step(
    const std::vector<glm::vec2>& polyline,
    const SimulationControlSearchDesc& desc)
{
    return follow_polyline_step(m_vehicle_state, polyline, desc);
}


std::vector<Vehicle::SimulationControlCandidate> Vehicle::find_best_simulation_controls(
    const VehicleTransformState& state,
    const std::vector<glm::vec2>& polyline) const
{
    return find_best_simulation_controls(state, polyline, SimulationControlSearchDesc{});
}

std::vector<Vehicle::SimulationControlCandidate> Vehicle::find_best_simulation_controls(
    const VehicleTransformState& state,
    const std::vector<glm::vec2>& polyline,
    const SimulationControlSearchDesc& desc) const
{
    LOG_METHOD();

    logger().check(desc.speed_acceleration_samples > 0, "speed_acceleration_samples must be greater than zero");
    logger().check(desc.steer_acceleration_samples > 0, "steer_acceleration_samples must be greater than zero");
    logger().check(desc.max_results > 0, "max_results must be greater than zero");
    logger().check(desc.simulation_time > 0.0f, "simulation_time must be greater than zero");
    logger().check(desc.dt > 0.0f, "dt must be greater than zero");

    std::vector<SimulationControlCandidate> candidates;
    candidates.reserve(
        static_cast<size_t>(desc.speed_acceleration_samples) *
        static_cast<size_t>(desc.steer_acceleration_samples)
    );

    for (int speed_id = 0; speed_id < desc.speed_acceleration_samples; speed_id++) {
        const float speed_acceleration = sample_symmetric_range(
            m_max_acceleration,
            speed_id,
            desc.speed_acceleration_samples
        );

        for (int steer_id = 0; steer_id < desc.steer_acceleration_samples; steer_id++) {
            const float steer_acceleration = sample_symmetric_range(
                m_max_steer_acceleration,
                steer_id,
                desc.steer_acceleration_samples
            );

            VehicleTransformState predicted_state = state;
            const float loss = compute_simulation_loss(
                predicted_state,
                polyline,
                speed_acceleration,
                steer_acceleration,
                desc.simulation_time,
                desc.dt,
                desc.debug
            );

            candidates.push_back(SimulationControlCandidate{
                .speed_acceleration = speed_acceleration,
                .steer_acceleration = steer_acceleration,
                .loss = loss,
                .predicted_state = predicted_state
            });
        }
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const SimulationControlCandidate& a, const SimulationControlCandidate& b) {
            return a.loss < b.loss;
        }
    );

    if (candidates.size() > static_cast<size_t>(desc.max_results)) {
        candidates.resize(static_cast<size_t>(desc.max_results));
    }

    return candidates;
}

std::vector<Vehicle::SimulationControlCandidate> Vehicle::find_best_simulation_controls(
    const std::vector<glm::vec2>& polyline) const
{
    return find_best_simulation_controls(m_vehicle_state, polyline, SimulationControlSearchDesc{});
}

std::vector<Vehicle::SimulationControlCandidate> Vehicle::find_best_simulation_controls(
    const std::vector<glm::vec2>& polyline,
    const SimulationControlSearchDesc& desc) const
{
    return find_best_simulation_controls(m_vehicle_state, polyline, desc);
}

void Vehicle::check_simulation_step(    
    float speed_acceleration,
    float steer_acceleration,
    float dt) const
{
    logger().check(dt > 0.0f, "dt must be greater than zero");
    logger().check(
        std::abs(speed_acceleration) <= m_max_acceleration,
        "abs(speed_acceleration) must be less than m_max_acceleration"
    );
    logger().check(
        std::abs(steer_acceleration) <= m_max_steer_acceleration,
        "abs(steer_acceleration) must be less than m_max_acceleration"
    );
}

void Vehicle::vehicle_simulation_step(
    VehicleTransformState& state,
    float speed_acceleration,
    float steer_acceleration,
    float dt,
    bool debug) const
{
    if (debug) {
        check_simulation_step(speed_acceleration, steer_acceleration, dt);
    }

    const float speed0 = state.m_speed;
    const float steer_angle0 = state.m_steer_angle;
    const float steering_angle_rate0 = state.m_steering_angle_rate;

    const glm::vec2 velocity0 =
        glm::vec2{std::cos(steer_angle0), std::sin(steer_angle0)} * speed0;

    const float speed1 = speed0 + speed_acceleration * dt;
    const float steering_angle_rate1 = steering_angle_rate0 + steer_acceleration * dt;
    const float steer_angle1 =
        steer_angle0 +
        steering_angle_rate0 * dt +
        0.5f * steer_acceleration * dt * dt;

    const glm::vec2 velocity1 =
        glm::vec2{std::cos(steer_angle1), std::sin(steer_angle1)} * speed1;

    state.m_position += 0.5f * (velocity0 + velocity1) * dt;
    state.m_speed = speed1;
    state.m_steer_angle = steer_angle1;
    state.m_steering_angle_rate = steering_angle_rate1;
}

void Vehicle::simulate_vehicle(
    VehicleTransformState& state,
    float speed_acceleration,
    float steer_acceleration,
    float simulation_time,
    float dt,
    bool debug) const
{
    LOG_METHOD();

    if (debug) {
        logger().check(simulation_time > 0.0f, "simulation_time must be greater than zero");
        check_simulation_step(speed_acceleration, steer_acceleration, dt);
    }

    for (float time = 0.0f; time < simulation_time;) {
        const float step_dt = std::min(dt, simulation_time - time);
        vehicle_simulation_step(state, speed_acceleration, steer_acceleration, step_dt, false);
        time += step_dt;
    }
}

Vehicle::PointProjection Vehicle::find_polyline_projection(
    const std::vector<glm::vec2>& polyline,
    glm::vec2 point,
    float min_s,
    float max_s)
{
    LOG_NAMED("Vehicle");

    logger().check(!polyline.empty(), "polyline must contain at least one point");
    logger().check(!std::isnan(min_s) && !std::isnan(max_s), "s range must not contain NaN");

    if (max_s < min_s) {
        std::swap(min_s, max_s);
    }

    if (polyline.size() == 1) {
        return PointProjection{
            .s = 0.0f,
            .dist = glm::length(point - polyline.front()),
            .point = polyline.front()
        };
    }

    constexpr float eps = 1e-6f;

    float total_length = 0.0f;
    for (size_t i = 1; i < polyline.size(); i++) {
        const float segment_length = glm::length(polyline[i] - polyline[i - 1]);
        if (segment_length > eps) {
            total_length += segment_length;
        }
    }

    if (total_length <= eps) {
        return PointProjection{
            .s = 0.0f,
            .dist = glm::length(point - polyline.front()),
            .point = polyline.front()
        };
    }

    const float clamped_min_s = std::clamp(min_s, 0.0f, total_length);
    const float clamped_max_s = std::clamp(max_s, 0.0f, total_length);

    PointProjection best_projection;
    float best_dist2 = std::numeric_limits<float>::infinity();
    float segment_start_s = 0.0f;

    for (size_t i = 1; i < polyline.size(); i++) {
        const glm::vec2 segment_start = polyline[i - 1];
        const glm::vec2 segment = polyline[i] - segment_start;
        const float segment_length = glm::length(segment);

        if (segment_length <= eps) {
            continue;
        }

        const float segment_end_s = segment_start_s + segment_length;

        if (segment_end_s < clamped_min_s || segment_start_s > clamped_max_s) {
            segment_start_s = segment_end_s;
            continue;
        }

        const float local_min_s = std::max(clamped_min_s, segment_start_s);
        const float local_max_s = std::min(clamped_max_s, segment_end_s);
        const float local_min_t = (local_min_s - segment_start_s) / segment_length;
        const float local_max_t = (local_max_s - segment_start_s) / segment_length;

        const float segment_len2 = glm::dot(segment, segment);
        const float unconstrained_t = glm::dot(point - segment_start, segment) / segment_len2;
        const float t = std::clamp(unconstrained_t, local_min_t, local_max_t);
        const glm::vec2 projected_point = segment_start + segment * t;
        const glm::vec2 diff = point - projected_point;
        const float dist2 = glm::dot(diff, diff);

        if (dist2 < best_dist2) {
            best_dist2 = dist2;
            best_projection = PointProjection{
                .s = segment_start_s + t * segment_length,
                .dist = std::sqrt(dist2),
                .point = projected_point
            };
        }

        segment_start_s = segment_end_s;
    }

    return best_projection;
}

float Vehicle::polyline_length(const std::vector<glm::vec2>& polyline)
{
    float length = 0.0f;

    for (size_t i = 1; i < polyline.size(); i++) {
        const float segment_length = glm::length(polyline[i] - polyline[i - 1]);
        if (segment_length > kEps) {
            length += segment_length;
        }
    }

    return length;
}

glm::vec2 Vehicle::polyline_tangent_at_s(const std::vector<glm::vec2>& polyline, float s)
{
    logger().check(polyline.size() >= 2, "polyline must contain at least two points");

    float segment_start_s = 0.0f;
    glm::vec2 last_valid_tangent = glm::vec2{1.0f, 0.0f};

    for (size_t i = 1; i < polyline.size(); i++) {
        const glm::vec2 segment = polyline[i] - polyline[i - 1];
        const float segment_length = glm::length(segment);

        if (segment_length <= kEps) {
            continue;
        }

        last_valid_tangent = segment / segment_length;
        const float segment_end_s = segment_start_s + segment_length;

        if (s <= segment_end_s + kEps) {
            return last_valid_tangent;
        }

        segment_start_s = segment_end_s;
    }

    return last_valid_tangent;
}

float Vehicle::angle_diff(float from, float to)
{
    constexpr float two_pi = 6.2831853071795864769f;
    constexpr float pi = 3.14159265358979323846f;

    float diff = std::fmod(to - from, two_pi);
    if (diff <= -pi) diff += two_pi;
    if (diff > pi) diff -= two_pi;
    return diff;
}

glm::vec2 Vehicle::forward_vector(const VehicleTransformState& state)
{
    return glm::vec2{std::cos(state.m_steer_angle), std::sin(state.m_steer_angle)};
}

float Vehicle::compute_position_loss_coefficient(
    const VehicleTransformState& state,
    const PointProjection& projection)
{
    const glm::vec2 diff = state.m_position - projection.point;
    return kPositionLossWeight * glm::dot(diff, diff);
}

float Vehicle::compute_heading_loss_coefficient(
    const VehicleTransformState& state,
    glm::vec2 path_tangent)
{
    const float path_angle = std::atan2(path_tangent.y, path_tangent.x);
    const float heading_error = angle_diff(state.m_steer_angle, path_angle);
    const float speed_factor = std::clamp(std::abs(state.m_speed) / kCruiseSpeed, 0.25f, 1.0f);

    return kHeadingLossWeight * speed_factor * heading_error * heading_error;
}

float Vehicle::compute_speed_loss_coefficient(
    const VehicleTransformState& state,
    const PointProjection& projection,
    glm::vec2 path_tangent,
    float total_polyline_length) const
{
    const float remaining_s = std::max(0.0f, total_polyline_length - projection.s);
    const float braking_speed = std::sqrt(std::max(0.0f, 2.0f * m_max_acceleration * remaining_s));
    float target_path_speed = std::min(kCruiseSpeed, braking_speed);

    const float off_path_speed_factor = std::clamp(
        1.0f - projection.dist / kSlowdownDistanceFromPath,
        kMinOffPathSpeedFactor,
        1.0f
    );
    target_path_speed *= off_path_speed_factor;

    const float path_speed = glm::dot(forward_vector(state) * state.m_speed, path_tangent);
    const float speed_error = path_speed - target_path_speed;

    return kSpeedLossWeight * speed_error * speed_error;
}

float Vehicle::compute_progress_loss_coefficient(
    const VehicleTransformState& state,
    glm::vec2 path_tangent)
{
    const float path_speed = glm::dot(forward_vector(state) * state.m_speed, path_tangent);
    const float backwards_speed = std::max(0.0f, -path_speed);

    return kBackwardProgressLossWeight * backwards_speed * backwards_speed;
}

float Vehicle::compute_control_loss_coefficient(
    const VehicleTransformState& state,
    float speed_acceleration,
    float steer_acceleration) const
{
    const float speed_acceleration_scale = std::max(m_max_acceleration, kEps);
    const float steer_acceleration_scale = std::max(m_max_steer_acceleration, kEps);
    const float normalized_speed_acceleration = speed_acceleration / speed_acceleration_scale;
    const float normalized_steer_acceleration = steer_acceleration / steer_acceleration_scale;

    return kControlLossWeight * (
        normalized_speed_acceleration * normalized_speed_acceleration +
        normalized_steer_acceleration * normalized_steer_acceleration +
        kSteeringRateLossWeight * state.m_steering_angle_rate * state.m_steering_angle_rate
    );
}

Vehicle::SimulationLossCoefficients Vehicle::compute_simulation_loss_coefficients(
    const VehicleTransformState& state,
    const PointProjection& projection,
    glm::vec2 path_tangent,
    float total_polyline_length,
    float speed_acceleration,
    float steer_acceleration) const
{
    return SimulationLossCoefficients{
        .position = compute_position_loss_coefficient(state, projection),
        .heading = compute_heading_loss_coefficient(state, path_tangent),
        .speed = compute_speed_loss_coefficient(state, projection, path_tangent, total_polyline_length),
        .progress = compute_progress_loss_coefficient(state, path_tangent),
        .control = compute_control_loss_coefficient(state, speed_acceleration, steer_acceleration)
    };
}

float Vehicle::compute_simulation_loss(
    VehicleTransformState& state,
    const std::vector<glm::vec2>& polyline,
    float speed_acceleration,
    float steer_acceleration,
    float simulation_time, 
    float dt,
    bool debug) const
{
    LOG_METHOD();

    if (debug) {
        logger().check(simulation_time > 0.0f, "simulation_time must be greater than zero");
        logger().check(polyline.size() >= 2, "polyline must contain at least two points");
        check_simulation_step(speed_acceleration, steer_acceleration, dt);
    }

    const float total_polyline_length = polyline_length(polyline);
    logger().check(total_polyline_length > kEps, "polyline length must be greater than zero");

    PointProjection previous_projection = find_polyline_projection(polyline, state.m_position);
    glm::vec2 previous_tangent = polyline_tangent_at_s(polyline, previous_projection.s);
    SimulationLossCoefficients previous_loss_coefficients = compute_simulation_loss_coefficients(
        state,
        previous_projection,
        previous_tangent,
        total_polyline_length,
        speed_acceleration,
        steer_acceleration
    );

    float loss = 0.0f;
    for (float time = 0.0f; time < simulation_time;) {
        const float step_dt = std::min(dt, simulation_time - time);
        const float projection_min_s = previous_projection.s - kProjectionBacktrackWindow;
        const float projection_max_s =
            previous_projection.s +
            kProjectionLookaheadBase +
            std::abs(state.m_speed) * step_dt +
            0.5f * std::abs(speed_acceleration) * step_dt * step_dt;

        vehicle_simulation_step(state, speed_acceleration, steer_acceleration, step_dt, false);

        PointProjection current_projection = find_polyline_projection(
            polyline,
            state.m_position,
            projection_min_s,
            projection_max_s
        );
        glm::vec2 current_tangent = polyline_tangent_at_s(polyline, current_projection.s);
        SimulationLossCoefficients current_loss_coefficients = compute_simulation_loss_coefficients(
            state,
            current_projection,
            current_tangent,
            total_polyline_length,
            speed_acceleration,
            steer_acceleration
        );

        loss += 0.5f * (
            previous_loss_coefficients.total() +
            current_loss_coefficients.total()
        ) * step_dt;

        previous_projection = current_projection;
        previous_loss_coefficients = current_loss_coefficients;
        time += step_dt;
    }

    return loss;
}
