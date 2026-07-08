#include "vehicle.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <utility>

#include "../vulkan_self/utils.h"

namespace {
    constexpr float kMaxSteeringAngle = 0.7f;
    constexpr float kSteeringVelocityLossFactor = 0.08f;
    constexpr float kProjectionBacktrackWindow = 0.75f;
    constexpr float kProjectionLookaheadBase = 6.0f;

    float previous_direction_change_s(
        const Vehicle::PathArcLengthTable& path_arc_lengths,
        float s)
    {
        const std::vector<float>& changes = path_arc_lengths.direction_change_s;
        if (changes.empty()) {
            return 0.0f;
        }

        const auto change_it = std::lower_bound(
            changes.begin(),
            changes.end(),
            s - Utils::eps
        );
        if (change_it == changes.begin()) {
            return 0.0f;
        }

        return *(change_it - 1);
    }

    float compute_virtual_remaining_s(
        const Vehicle::PathArcLengthTable& path_arc_lengths,
        float current_s,
        float stop_s,
        float min_virtual_segment_length)
    {
        const float real_remaining_s = std::max(0.0f, stop_s - current_s);
        if (real_remaining_s <= Utils::eps || min_virtual_segment_length <= Utils::eps) {
            return real_remaining_s;
        }

        const float segment_start_s = previous_direction_change_s(path_arc_lengths, current_s);
        const float segment_length = std::max(stop_s - segment_start_s, Utils::eps);
        if (segment_length >= min_virtual_segment_length) {
            return real_remaining_s;
        }

        return real_remaining_s * min_virtual_segment_length / segment_length;
    }
}

Vehicle::Vehicle(
    const VehicleTransformState& initial_state,
    float max_acceleration,
    float max_steer_acceleration,
    float wheel_base)
    :   m_vehicle_state(initial_state),
        m_max_acceleration(max_acceleration),
        m_max_steer_acceleration(max_steer_acceleration),
        m_wheel_base(wheel_base)
{
    logger().check(m_wheel_base > Utils::eps, "wheel_base must be greater than zero");
}

Vehicle::Vehicle(float max_acceleration, float max_steer_acceleration, float wheel_base)
    :   Vehicle(
            VehicleTransformState{}, 
            max_acceleration, 
            max_steer_acceleration,
            wheel_base
        ) {}

void Vehicle::simulate_vehicle(
    float speed_acceleration,
    float steer_acceleration,
    float simulation_time, 
    float dt,
    bool debug) 
{
    simulate_vehicle(m_vehicle_state, speed_acceleration, steer_acceleration, simulation_time, dt, debug);
}

Vehicle::SimulationLossWeights& Vehicle::loss_weights() noexcept {
    return m_loss_weights;
}

const Vehicle::SimulationLossWeights& Vehicle::loss_weights() const noexcept {
    return m_loss_weights;
}

void Vehicle::reset_loss_weights() noexcept {
    m_loss_weights = SimulationLossWeights{};
}

Vehicle::SimulationFollowParams& Vehicle::follow_params() noexcept {
    return m_follow_params;
}

const Vehicle::SimulationFollowParams& Vehicle::follow_params() const noexcept {
    return m_follow_params;
}

void Vehicle::reset_follow_params() noexcept {
    m_follow_params = SimulationFollowParams{};
}

float Vehicle::path_potential_distance_exponent() const noexcept {
    return m_path_potential_distance_exponent;
}

void Vehicle::set_path_potential_distance_exponent(float exponent) noexcept {
    m_path_potential_distance_exponent =
        std::isfinite(exponent) ? std::max(0.0f, exponent) : 0.5f;
}

Vehicle::PolylineFollowStepResult Vehicle::follow_polyline_step(
    VehicleTransformState& state,
    const std::vector<glm::vec2>& polyline) const
{
    return follow_polyline_step(state, make_forward_vehicle_path(polyline), SimulationControlSearchDesc{});
}

Vehicle::PolylineFollowStepResult Vehicle::follow_polyline_step(
    VehicleTransformState& state,
    const std::vector<VehiclePathPoint>& path) const
{
    return follow_polyline_step(state, path, SimulationControlSearchDesc{});
}

Vehicle::PolylineFollowStepResult Vehicle::follow_polyline_step(
    VehicleTransformState& state,
    const std::vector<glm::vec2>& polyline,
    const SimulationControlSearchDesc& desc) const
{
    return follow_polyline_step(state, make_forward_vehicle_path(polyline), desc);
}

Vehicle::PolylineFollowStepResult Vehicle::follow_polyline_step(
    VehicleTransformState& state,
    const std::vector<VehiclePathPoint>& path,
    const SimulationControlSearchDesc& desc) const
{
    LOG_METHOD();

    std::vector<SimulationControlCandidate> candidates = find_best_simulation_controls(state, path, desc);
    logger().check(!candidates.empty(), "no simulation control candidates found");

    SimulationControlCandidate selected_control = candidates.front();
    vehicle_simulation_step(
        state,
        selected_control.control_command.speed_acceleration,
        selected_control.control_command.steer_acceleration,
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
    return follow_polyline_step(m_vehicle_state, make_forward_vehicle_path(polyline), SimulationControlSearchDesc{});
}

Vehicle::PolylineFollowStepResult Vehicle::follow_polyline_step(
    const std::vector<VehiclePathPoint>& path)
{
    return follow_polyline_step(m_vehicle_state, path, SimulationControlSearchDesc{});
}

Vehicle::PolylineFollowStepResult Vehicle::follow_polyline_step(
    const std::vector<glm::vec2>& polyline,
    const SimulationControlSearchDesc& desc)
{
    return follow_polyline_step(m_vehicle_state, make_forward_vehicle_path(polyline), desc);
}

Vehicle::PolylineFollowStepResult Vehicle::follow_polyline_step(
    const std::vector<VehiclePathPoint>& path,
    const SimulationControlSearchDesc& desc)
{
    return follow_polyline_step(m_vehicle_state, path, desc);
}


std::vector<Vehicle::SimulationControlCandidate> Vehicle::find_best_simulation_controls(
    const VehicleTransformState& state,
    const std::vector<glm::vec2>& polyline) const
{
    return find_best_simulation_controls(state, make_forward_vehicle_path(polyline), SimulationControlSearchDesc{});
}

std::vector<Vehicle::SimulationControlCandidate> Vehicle::find_best_simulation_controls(
    const VehicleTransformState& state,
    const std::vector<VehiclePathPoint>& path) const
{
    return find_best_simulation_controls(state, path, SimulationControlSearchDesc{});
}

std::vector<Vehicle::SimulationControlCandidate> Vehicle::find_best_simulation_controls(
    const VehicleTransformState& state,
    const std::vector<glm::vec2>& polyline,
    const SimulationControlSearchDesc& desc) const
{
    return find_best_simulation_controls(state, make_forward_vehicle_path(polyline), desc);
}

std::vector<Vehicle::SimulationControlCandidate> Vehicle::find_best_simulation_controls(
    const VehicleTransformState& state,
    const std::vector<VehiclePathPoint>& path,
    const SimulationControlSearchDesc& desc) const
{
    return find_best_simulation_controls(state, path, build_path_arc_length_table(path), desc);
}

std::vector<Vehicle::SimulationControlCandidate> Vehicle::find_best_simulation_controls(
    const VehicleTransformState& state,
    const std::vector<VehiclePathPoint>& path,
    const PathArcLengthTable& path_arc_lengths,
    const SimulationControlSearchDesc& desc) const
{
    logger().check(desc.speed_acceleration_samples > 0, "speed_acceleration_samples must be greater than zero");
    logger().check(desc.steer_acceleration_samples > 0, "steer_acceleration_samples must be greater than zero");
    logger().check(desc.max_results > 0, "max_results must be greater than zero");
    logger().check(desc.simulation_time > 0.0f, "simulation_time must be greater than zero");
    logger().check(desc.dt > 0.0f, "dt must be greater than zero");

    if (path_arc_lengths.point_s.size() != path.size()) {
        return find_best_simulation_controls(state, path, build_path_arc_length_table(path), desc);
    }

    const float total_polyline_length = polyline_length(path_arc_lengths);
    if (total_polyline_length <= Utils::eps) {
        return std::vector<SimulationControlCandidate>();
    }

    std::vector<SimulationControlCandidate> candidates;
    candidates.reserve(
        static_cast<size_t>(desc.speed_acceleration_samples) *
        static_cast<size_t>(desc.steer_acceleration_samples)
    );

    for (int speed_id = 0; speed_id < desc.speed_acceleration_samples; speed_id++) {
        const float speed_acceleration = Utils::sample_symmetric_range(
            m_max_acceleration,
            speed_id,
            desc.speed_acceleration_samples
        );

        for (int steer_id = 0; steer_id < desc.steer_acceleration_samples; steer_id++) {
            const float steer_acceleration = Utils::sample_symmetric_range(
                m_max_steer_acceleration,
                steer_id,
                desc.steer_acceleration_samples
            );

            VehicleTransformState predicted_state = state;
            std::vector<glm::vec2> trajectory;
            SimulationControlCandidateDebug candidate_debug;
            const float loss = compute_simulation_loss_test(
                predicted_state,
                path,
                path_arc_lengths,
                total_polyline_length,
                speed_acceleration,
                steer_acceleration,
                desc.simulation_time,
                desc.dt,
                desc.debug,
                desc.initial_projection_min_s,
                desc.initial_projection_max_s,
                desc.slow_down_at_projection_max,
                desc.projection_max_target_speed_abs,
                &trajectory,
                &candidate_debug
            );

            candidates.push_back(SimulationControlCandidate{
                .control_command = VehicleControlCommand{
                    .speed_acceleration = speed_acceleration,
                    .steer_acceleration = steer_acceleration
                },
                .loss = loss,
                .predicted_state = predicted_state,
                .trajectory = std::move(trajectory),
                .debug = candidate_debug
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
    return find_best_simulation_controls(m_vehicle_state, make_forward_vehicle_path(polyline), SimulationControlSearchDesc{});
}

std::vector<Vehicle::SimulationControlCandidate> Vehicle::find_best_simulation_controls(
    const std::vector<VehiclePathPoint>& path) const
{
    return find_best_simulation_controls(m_vehicle_state, path, SimulationControlSearchDesc{});
}

std::vector<Vehicle::SimulationControlCandidate> Vehicle::find_best_simulation_controls(
    const std::vector<glm::vec2>& polyline,
    const SimulationControlSearchDesc& desc) const
{
    return find_best_simulation_controls(m_vehicle_state, make_forward_vehicle_path(polyline), desc);
}

std::vector<Vehicle::SimulationControlCandidate> Vehicle::find_best_simulation_controls(
    const std::vector<VehiclePathPoint>& path,
    const SimulationControlSearchDesc& desc) const
{
    return find_best_simulation_controls(m_vehicle_state, path, desc);
}

std::vector<Vehicle::SimulationControlCandidate> Vehicle::find_best_simulation_controls(
    const std::vector<VehiclePathPoint>& path,
    const PathArcLengthTable& path_arc_lengths,
    const SimulationControlSearchDesc& desc) const
{
    return find_best_simulation_controls(m_vehicle_state, path, path_arc_lengths, desc);
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
        "abs(steer_acceleration) must be less than m_max_steer_acceleration"
    );
}

Vehicle::VehicleTransformState& Vehicle::state() noexcept {
    return m_vehicle_state;
}

const Vehicle::VehicleTransformState& Vehicle::state() const noexcept {
    return m_vehicle_state;
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
    const float heading0 = state.m_heading;
    const float steering_angle0 = std::clamp(
        state.m_steering_angle,
        -kMaxSteeringAngle,
        kMaxSteeringAngle
    );
    float steering_angle_velocity0 = state.m_steering_angle_velocity;
    if ((steering_angle0 <= -kMaxSteeringAngle + Utils::eps && steering_angle_velocity0 < 0.0f) ||
        (steering_angle0 >= kMaxSteeringAngle - Utils::eps && steering_angle_velocity0 > 0.0f))
    {
        steering_angle_velocity0 = 0.0f;
    }

    const glm::vec2 velocity0 =
        glm::vec2{std::cos(heading0), std::sin(heading0)} * speed0;
    const float heading_velocity0 = speed0 * std::tan(steering_angle0) / m_wheel_base;

    const float speed1 = speed0 + speed_acceleration * dt;
    float steering_angle_velocity1 = steering_angle_velocity0 + steer_acceleration * dt;
    float steering_angle1 =
        steering_angle0 +
        steering_angle_velocity0 * dt +
        0.5f * steer_acceleration * dt * dt;

    steering_angle1 = std::clamp(
        steering_angle1,
        -kMaxSteeringAngle,
        kMaxSteeringAngle
    );
    if ((steering_angle1 <= -kMaxSteeringAngle + Utils::eps && steering_angle_velocity1 < 0.0f) ||
        (steering_angle1 >= kMaxSteeringAngle - Utils::eps && steering_angle_velocity1 > 0.0f))
    {
        steering_angle_velocity1 = 0.0f;
    }

    const float heading_velocity1 = speed1 * std::tan(steering_angle1) / m_wheel_base;
    const float heading1 = heading0 + 0.5f * (heading_velocity0 + heading_velocity1) * dt;

    const glm::vec2 velocity1 =
        glm::vec2{std::cos(heading1), std::sin(heading1)} * speed1;

    state.m_position += 0.5f * (velocity0 + velocity1) * dt;
    state.m_speed = speed1;
    state.m_speed_acceleration = speed_acceleration;
    state.m_heading = heading1;
    state.m_steering_angle = steering_angle1;
    state.m_steering_angle_velocity = steering_angle_velocity1;
    state.m_steering_angle_acceleration = steer_acceleration;
}

Vehicle::VehicleTransformState Vehicle::get_vehicle_simulation_step(
    const VehicleTransformState& state,
    float speed_acceleration,
    float steer_acceleration,
    float dt,
    bool debug) const
{
    VehicleTransformState result = state;
    vehicle_simulation_step(result, speed_acceleration, steer_acceleration, dt, debug);
    return result;
}

void Vehicle::vehicle_simulation_step(
    float speed_acceleration,
    float steer_acceleration,
    float dt,
    bool debug)
{
    vehicle_simulation_step(m_vehicle_state, speed_acceleration, steer_acceleration, dt, debug);
}

Vehicle::VehicleTransformState Vehicle::get_vehicle_simulation_step(
    float speed_acceleration,
    float steer_acceleration,
    float dt,
    bool debug)
{
    return get_vehicle_simulation_step(m_vehicle_state, speed_acceleration, steer_acceleration, dt, debug);
}

void Vehicle::simulate_vehicle(
    VehicleTransformState& state,
    float speed_acceleration,
    float steer_acceleration,
    float simulation_time,
    float dt,
    bool debug) const
{
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
    const std::vector<VehiclePathPoint>& path,
    glm::vec2 point,
    float min_s,
    float max_s)
{
    return find_polyline_projection(
        path,
        build_path_arc_length_table(path),
        point,
        min_s,
        max_s
    );
}

Vehicle::PointProjection Vehicle::find_polyline_projection(
    const std::vector<VehiclePathPoint>& path,
    const PathArcLengthTable& path_arc_lengths,
    glm::vec2 point,
    float min_s,
    float max_s)
{
    logger().check(!path.empty(), "path must contain at least one point");
    logger().check(!std::isnan(min_s) && !std::isnan(max_s), "s range must not contain NaN");

    if (path_arc_lengths.point_s.size() != path.size()) {
        return find_polyline_projection(
            path,
            build_path_arc_length_table(path),
            point,
            min_s,
            max_s
        );
    }

    if (max_s < min_s) {
        std::swap(min_s, max_s);
    }

    if (path.size() == 1) {
        const glm::vec2 tangent = glm::vec2{
            std::cos(path.front().heading),
            std::sin(path.front().heading)
        };

        return PointProjection{
            .s = 0.0f,
            .dist = glm::length(point - path.front().position),
            .point = path.front().position,
            .tangent = tangent,
            .heading = path.front().heading,
            .dir = Utils::normalized_dir(path.front().dir)
        };
    }

    constexpr float eps = 1e-6f;

    if (path_arc_lengths.total_length <= eps) {
        const glm::vec2 tangent = glm::vec2{
            std::cos(path.front().heading),
            std::sin(path.front().heading)
        };

        return PointProjection{
            .s = 0.0f,
            .dist = glm::length(point - path.front().position),
            .point = path.front().position,
            .tangent = tangent,
            .heading = path.front().heading,
            .dir = Utils::normalized_dir(path.front().dir)
        };
    }

    const float clamped_min_s = std::clamp(min_s, 0.0f, path_arc_lengths.total_length);
    const float clamped_max_s = std::clamp(max_s, 0.0f, path_arc_lengths.total_length);

    PointProjection best_projection;
    float best_dist2 = std::numeric_limits<float>::infinity();
    const std::vector<float>& point_s = path_arc_lengths.point_s;
    const auto first_segment_it = std::lower_bound(
        point_s.begin() + 1,
        point_s.end(),
        clamped_min_s
    );
    const size_t first_segment_id = first_segment_it == point_s.end()
        ? path.size() - 1
        : static_cast<size_t>(first_segment_it - point_s.begin());

    for (size_t i = std::max<size_t>(1, first_segment_id); i < path.size(); i++) {
        const glm::vec2 segment_start = path[i - 1].position;
        const glm::vec2 segment = path[i].position - segment_start;
        const float segment_start_s = point_s[i - 1];
        const float segment_end_s = point_s[i];
        const float segment_length = segment_end_s - segment_start_s;

        if (segment_length <= eps) {
            continue;
        }

        if (segment_end_s < clamped_min_s || segment_start_s > clamped_max_s) {
            if (segment_start_s > clamped_max_s) {
                break;
            }
            continue;
        }

        const float local_min_s = std::max(clamped_min_s, segment_start_s);
        const float local_max_s = std::min(clamped_max_s, segment_end_s);
        if (local_max_s - local_min_s <= Utils::eps &&
            clamped_max_s - clamped_min_s > Utils::eps)
        {
            continue;
        }

        const float local_min_t = (local_min_s - segment_start_s) / segment_length;
        const float local_max_t = (local_max_s - segment_start_s) / segment_length;

        const float segment_len2 = glm::dot(segment, segment);
        const float unconstrained_t = glm::dot(point - segment_start, segment) / segment_len2;
        const float t = std::clamp(unconstrained_t, local_min_t, local_max_t);
        const glm::vec2 projected_point = segment_start + segment * t;
        const glm::vec2 diff = point - projected_point;
        const float dist2 = glm::dot(diff, diff);

        if (dist2 < best_dist2) {
            const glm::vec2 tangent = segment / segment_length;
            const float heading = Utils::lerp_angle(path[i - 1].heading, path[i].heading, t);
            const float dir = Utils::normalized_dir(path[i].dir);

            best_dist2 = dist2;
            best_projection = PointProjection{
                .s = segment_start_s + t * segment_length,
                .dist = std::sqrt(dist2),
                .point = projected_point,
                .tangent = tangent,
                .heading = heading,
                .dir = dir
            };
        }
    }

    if (!std::isfinite(best_dist2)) {
        best_projection = sample_polyline_at_s(path, path_arc_lengths, clamped_min_s);
        best_projection.dist = glm::length(point - best_projection.point);
    }

    return best_projection;
}

Vehicle::PointProjection Vehicle::sample_polyline_at_s(
    const std::vector<VehiclePathPoint>& path,
    float s)
{
    return sample_polyline_at_s(path, build_path_arc_length_table(path), s);
}

Vehicle::PointProjection Vehicle::sample_polyline_at_s(
    const std::vector<VehiclePathPoint>& path,
    const PathArcLengthTable& path_arc_lengths,
    float s)
{
    logger().check(!path.empty(), "path must contain at least one point");
    logger().check(!std::isnan(s), "s must not be NaN");

    if (path_arc_lengths.point_s.size() != path.size()) {
        return sample_polyline_at_s(path, build_path_arc_length_table(path), s);
    }

    const auto point_projection_from_path_point =
        [](const VehiclePathPoint& path_point, float point_s) {
            const glm::vec2 tangent = glm::vec2{
                std::cos(path_point.heading),
                std::sin(path_point.heading)
            };

            return PointProjection{
                .s = point_s,
                .dist = 0.0f,
                .point = path_point.position,
                .tangent = tangent,
                .heading = path_point.heading,
                .dir = Utils::normalized_dir(path_point.dir)
            };
        };

    if (path.size() == 1) {
        return point_projection_from_path_point(path.front(), 0.0f);
    }

    const float total_length = path_arc_lengths.total_length;
    if (total_length <= Utils::eps) {
        return point_projection_from_path_point(path.front(), 0.0f);
    }

    const float clamped_s = std::clamp(s, 0.0f, total_length);
    const std::vector<float>& point_s = path_arc_lengths.point_s;
    const auto segment_it = std::lower_bound(
        point_s.begin() + 1,
        point_s.end(),
        clamped_s
    );
    const size_t first_segment_id = segment_it == point_s.end()
        ? path.size() - 1
        : static_cast<size_t>(segment_it - point_s.begin());

    for (size_t i = std::max<size_t>(1, first_segment_id); i < path.size(); i++) {
        const glm::vec2 segment_start = path[i - 1].position;
        const glm::vec2 segment = path[i].position - segment_start;
        const float segment_start_s = point_s[i - 1];
        const float segment_end_s = point_s[i];
        const float segment_length = segment_end_s - segment_start_s;

        if (segment_length <= Utils::eps) {
            continue;
        }

        if (clamped_s <= segment_end_s || i + 1 == path.size()) {
            const float t = std::clamp(
                (clamped_s - segment_start_s) / segment_length,
                0.0f,
                1.0f
            );
            const glm::vec2 tangent = segment / segment_length;

            return PointProjection{
                .s = segment_start_s + t * segment_length,
                .dist = 0.0f,
                .point = segment_start + segment * t,
                .tangent = tangent,
                .heading = Utils::lerp_angle(path[i - 1].heading, path[i].heading, t),
                .dir = Utils::normalized_dir(path[i].dir)
            };
        }
    }

    return point_projection_from_path_point(path.back(), total_length);
}

Vehicle::PointProjection Vehicle::find_path_projection(
    const VehicleTransformState& state,
    const std::vector<VehiclePathPoint>& path,
    const PathArcLengthTable& path_arc_lengths,
    float min_s,
    float max_s) const
{
    logger().check(!path.empty(), "path must contain at least one point");
    logger().check(!std::isnan(min_s) && !std::isnan(max_s), "s range must not contain NaN");

    if (path_arc_lengths.point_s.size() != path.size()) {
        return find_path_projection(
            state,
            path,
            build_path_arc_length_table(path),
            min_s,
            max_s
        );
    }

    if (max_s < min_s) {
        std::swap(min_s, max_s);
    }

    const float total_length = path_arc_lengths.total_length;
    if (path.size() == 1 || total_length <= Utils::eps) {
        PointProjection projection = sample_polyline_at_s(path, path_arc_lengths, 0.0f);
        projection.dist = glm::length(state.m_position - projection.point);
        return projection;
    }

    const float clamped_min_s = std::clamp(min_s, 0.0f, total_length);
    const float clamped_max_s = std::clamp(max_s, 0.0f, total_length);
    const std::vector<float>& point_s = path_arc_lengths.point_s;
    const auto first_segment_it = std::lower_bound(
        point_s.begin() + 1,
        point_s.end(),
        clamped_min_s
    );
    const size_t first_segment_id = first_segment_it == point_s.end()
        ? path.size() - 1
        : static_cast<size_t>(first_segment_it - point_s.begin());

    PointProjection best_projection;
    float best_score = std::numeric_limits<float>::infinity();
    const float heading_distance_scale = std::max(m_wheel_base, 1.0f);

    for (size_t i = std::max<size_t>(1, first_segment_id); i < path.size(); i++) {
        const glm::vec2 segment_start = path[i - 1].position;
        const glm::vec2 segment = path[i].position - segment_start;
        const float segment_start_s = point_s[i - 1];
        const float segment_end_s = point_s[i];
        const float segment_length = segment_end_s - segment_start_s;

        if (segment_length <= Utils::eps) {
            continue;
        }
        if (segment_end_s < clamped_min_s || segment_start_s > clamped_max_s) {
            if (segment_start_s > clamped_max_s) {
                break;
            }
            continue;
        }

        const float local_min_s = std::max(clamped_min_s, segment_start_s);
        const float local_max_s = std::min(clamped_max_s, segment_end_s);
        const float local_min_t = (local_min_s - segment_start_s) / segment_length;
        const float local_max_t = (local_max_s - segment_start_s) / segment_length;

        const float segment_len2 = glm::dot(segment, segment);
        const float unconstrained_t =
            glm::dot(state.m_position - segment_start, segment) / segment_len2;
        const float t = std::clamp(unconstrained_t, local_min_t, local_max_t);
        const glm::vec2 projected_point = segment_start + segment * t;
        const glm::vec2 diff = state.m_position - projected_point;
        const float dist2 = glm::dot(diff, diff);
        const float heading = Utils::lerp_angle(path[i - 1].heading, path[i].heading, t);
        const float heading_error = angle_diff(state.m_heading, heading);
        const float heading_score =
            heading_distance_scale * heading_error *
            heading_distance_scale * heading_error;
        const float score = dist2 + heading_score;

        if (score < best_score) {
            const glm::vec2 tangent = segment / segment_length;
            best_score = score;
            best_projection = PointProjection{
                .s = segment_start_s + t * segment_length,
                .dist = std::sqrt(dist2),
                .point = projected_point,
                .tangent = tangent,
                .heading = heading,
                .dir = Utils::normalized_dir(path[i].dir)
            };
        }
    }

    if (!std::isfinite(best_score)) {
        best_projection = sample_polyline_at_s(path, path_arc_lengths, clamped_min_s);
        best_projection.dist = glm::length(state.m_position - best_projection.point);
    }

    return best_projection;
}

Vehicle::PathArcLengthTable Vehicle::build_path_arc_length_table(
    const std::vector<VehiclePathPoint>& path)
{
    PathArcLengthTable result;
    result.point_s.resize(path.size(), 0.0f);

    float previous_segment_dir = 1.0f;
    bool has_previous_segment = false;
    for (size_t i = 1; i < path.size(); i++) {
        const float segment_length = glm::length(path[i].position - path[i - 1].position);
        if (segment_length <= Utils::eps) {
            result.point_s[i] = result.total_length;
            continue;
        }

        const float segment_dir = Utils::normalized_dir(path[i].dir);
        if (has_previous_segment && segment_dir != previous_segment_dir) {
            result.direction_change_s.push_back(result.total_length);
        }

        previous_segment_dir = segment_dir;
        has_previous_segment = true;
        result.total_length += segment_length;
        result.point_s[i] = result.total_length;
    }

    return result;
}

float Vehicle::polyline_length(const std::vector<VehiclePathPoint>& path)
{
    return build_path_arc_length_table(path).total_length;
}

float Vehicle::polyline_length(const PathArcLengthTable& path_arc_lengths) noexcept
{
    return path_arc_lengths.total_length;
}

float Vehicle::next_direction_change_s(
    const std::vector<VehiclePathPoint>& path,
    float s,
    bool include_current)
{
    return next_direction_change_s(path, build_path_arc_length_table(path), s, include_current);
}

float Vehicle::next_direction_change_s(
    const std::vector<VehiclePathPoint>& path,
    const PathArcLengthTable& path_arc_lengths,
    float s,
    bool include_current)
{
    if (path_arc_lengths.point_s.size() != path.size()) {
        return next_direction_change_s(path, build_path_arc_length_table(path), s, include_current);
    }

    if (path_arc_lengths.direction_change_s.empty()) {
        return std::numeric_limits<float>::infinity();
    }

    const auto& changes = path_arc_lengths.direction_change_s;
    const auto change_it = include_current
        ? std::lower_bound(changes.begin(), changes.end(), s - Utils::eps)
        : std::upper_bound(changes.begin(), changes.end(), s + Utils::eps);
    if (change_it == changes.end()) {
        return std::numeric_limits<float>::infinity();
    }

    return *change_it;
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
    return glm::vec2{std::cos(state.m_heading), std::sin(state.m_heading)};
}

float Vehicle::compute_reference_progress(
    float initial_path_speed,
    float time) const
{
    if (time <= 0.0f) {
        return 0.0f;
    }

    const float acceleration = std::max(m_max_acceleration, Utils::eps);
    const float speed0 = std::max(0.0f, initial_path_speed);
    const float cruise_speed = std::max(0.0f, m_follow_params.cruise_speed);
    if (cruise_speed <= Utils::eps) {
        return 0.0f;
    }

    if (speed0 < cruise_speed) {
        const float acceleration_time = std::min(time, (cruise_speed - speed0) / acceleration);
        const float cruise_time = time - acceleration_time;
        return
            speed0 * acceleration_time +
            0.5f * acceleration * acceleration_time * acceleration_time +
            cruise_speed * cruise_time;
    }

    const float deceleration_time = std::min(time, (speed0 - cruise_speed) / acceleration);
    const float cruise_time = time - deceleration_time;
    return
        speed0 * deceleration_time -
        0.5f * acceleration * deceleration_time * deceleration_time +
        cruise_speed * cruise_time;
}

float Vehicle::compute_position_loss_coefficient(
    const VehicleTransformState& state,
    const PointProjection& target_projection) const
{
    const glm::vec2 diff = state.m_position - target_projection.point;
    return m_loss_weights.position * glm::dot(diff, diff);
}

float Vehicle::compute_heading_loss_coefficient(
    const VehicleTransformState& state,
    const PointProjection& target_projection) const
{
    const float heading_error = angle_diff(state.m_heading, target_projection.heading);
    const float cruise_speed = std::max(m_follow_params.cruise_speed, Utils::eps);
    const float speed_factor = std::clamp(std::abs(state.m_speed) / cruise_speed, 0.25f, 1.0f);

    return m_loss_weights.heading * speed_factor * heading_error * heading_error;
}

float Vehicle::compute_speed_loss_coefficient(
    const VehicleTransformState& state,
    const PointProjection& projection,
    const PathArcLengthTable& path_arc_lengths,
    float total_polyline_length,
    float current_next_direction_change_s) const
{
    float remaining_s = std::max(0.0f, total_polyline_length - projection.s);
    const bool has_direction_switch = std::isfinite(current_next_direction_change_s);
    if (has_direction_switch) {
        remaining_s = compute_virtual_remaining_s(
            path_arc_lengths,
            projection.s,
            current_next_direction_change_s,
            m_follow_params.min_direction_segment_virtual_length
        );
    }

    const float braking_speed = std::sqrt(std::max(0.0f, 2.0f * m_max_acceleration * remaining_s));
    const float cruise_speed = std::max(0.0f, m_follow_params.cruise_speed);
    float target_speed_abs = std::min(cruise_speed, braking_speed);
    if (has_direction_switch) {
        target_speed_abs = std::max(
            target_speed_abs,
            std::min(cruise_speed, std::max(0.0f, m_follow_params.direction_switch_approach_speed))
        );
    }

    float off_path_speed_factor = 1.0f;
    if (m_follow_params.slowdown_distance_from_path > Utils::eps) {
        off_path_speed_factor = std::clamp(
            1.0f - projection.dist / m_follow_params.slowdown_distance_from_path,
            std::clamp(m_follow_params.min_off_path_speed_factor, 0.0f, 1.0f),
            1.0f
        );
    }
    target_speed_abs *= off_path_speed_factor;

    const float target_speed = target_speed_abs * projection.dir;
    const float speed_error = state.m_speed - target_speed;

    return m_loss_weights.speed * speed_error * speed_error;
}

float Vehicle::compute_progress_loss_coefficient(
    const VehicleTransformState& state,
    const PointProjection& projection,
    const PointProjection& target_projection,
    float progress_reference_s) const
{
    const float path_speed = glm::dot(forward_vector(state) * state.m_speed, projection.tangent);
    const float backwards_speed = std::max(0.0f, -path_speed);
    const float forward_progress = std::max(0.0f, projection.s - progress_reference_s);
    const float progress_error = projection.s - target_projection.s;

    return
        m_loss_weights.progress_tracking * progress_error * progress_error +
        m_loss_weights.backward_progress * backwards_speed * backwards_speed -
        m_loss_weights.forward_progress * forward_progress;
}

float Vehicle::compute_steering_loss_coefficient(
    const VehicleTransformState& state,
    const PointProjection& target_projection) const
{
    const glm::vec2 to_target = target_projection.point - state.m_position;
    const float target_distance = glm::length(to_target);
    if (target_distance <= Utils::eps) {
        return m_loss_weights.steering *
            kSteeringVelocityLossFactor *
            state.m_steering_angle_velocity *
            state.m_steering_angle_velocity;
    }

    const float target_heading = std::atan2(to_target.y, to_target.x);
    const float motion_heading = target_projection.dir < 0.0f
        ? state.m_heading + 3.14159265358979323846f
        : state.m_heading;
    const float heading_error = angle_diff(motion_heading, target_heading);

    const float target_steering_angle = std::clamp(
        std::atan2(2.0f * m_wheel_base * std::sin(heading_error), target_distance) *
            target_projection.dir,
        -kMaxSteeringAngle,
        kMaxSteeringAngle
    );
    const float steering_error = angle_diff(state.m_steering_angle, target_steering_angle);

    return m_loss_weights.steering * (
        steering_error * steering_error +
        kSteeringVelocityLossFactor *
            state.m_steering_angle_velocity *
            state.m_steering_angle_velocity
    );
}

float Vehicle::compute_control_loss_coefficient(
    const VehicleTransformState& state,
    float speed_acceleration,
    float steer_acceleration) const
{
    const float speed_acceleration_scale = std::max(m_max_acceleration, Utils::eps);
    const float steer_acceleration_scale = std::max(m_max_steer_acceleration, Utils::eps);
    const float normalized_speed_acceleration = speed_acceleration / speed_acceleration_scale;
    const float normalized_steer_acceleration = steer_acceleration / steer_acceleration_scale;

    return m_loss_weights.control * (
        normalized_speed_acceleration * normalized_speed_acceleration +
        normalized_steer_acceleration * normalized_steer_acceleration +
        m_loss_weights.steering_rate * state.m_steering_angle_velocity * state.m_steering_angle_velocity
    );
}

float Vehicle::compute_potential_control_regularization(
    const VehicleTransformState& state,
    float speed_acceleration,
    float steer_acceleration) const
{
    return compute_control_loss_coefficient(state, speed_acceleration, steer_acceleration);
}

float Vehicle::compute_path_target_speed(
    const PointProjection& projection,
    float active_segment_end_s,
    bool slow_down_at_projection_max,
    float projection_max_target_speed_abs) const
{
    const float cruise_speed = std::max(0.0f, m_follow_params.cruise_speed);
    float target_speed_abs = cruise_speed;

    if (slow_down_at_projection_max) {
        const float remaining_s = std::max(0.0f, active_segment_end_s - projection.s);
        const float target_at_end =
            std::max(0.0f, projection_max_target_speed_abs);
        const float reachable_speed =
            std::sqrt(
                target_at_end * target_at_end +
                2.0f * std::max(m_max_acceleration, Utils::eps) * remaining_s
            );
        target_speed_abs = std::min(target_speed_abs, reachable_speed);
    }

    float off_path_speed_factor = 1.0f;
    if (m_follow_params.slowdown_distance_from_path > Utils::eps) {
        off_path_speed_factor = std::clamp(
            1.0f - projection.dist / m_follow_params.slowdown_distance_from_path,
            std::clamp(m_follow_params.min_off_path_speed_factor, 0.0f, 1.0f),
            1.0f
        );
    }
    target_speed_abs *= off_path_speed_factor;

    return target_speed_abs * projection.dir;
}

Vehicle::PathPotentialEvaluation Vehicle::evaluate_path_potential(
    const VehicleTransformState& state,
    const std::vector<VehiclePathPoint>& path,
    const PathArcLengthTable& path_arc_lengths,
    float active_segment_min_s,
    float active_segment_max_s,
    bool slow_down_at_projection_max,
    float projection_max_target_speed_abs,
    float speed_acceleration,
    float steer_acceleration) const
{
    PointProjection projection = find_path_projection(
        state,
        path,
        path_arc_lengths,
        active_segment_min_s,
        active_segment_max_s
    );

    const float active_end_s =
        std::clamp(
            active_segment_max_s,
            active_segment_min_s,
            std::max(active_segment_min_s, path_arc_lengths.total_length)
        );
    const float remaining_s = std::max(0.0f, active_end_s - projection.s);
    const float heading_error = angle_diff(state.m_heading, projection.heading);
    const float heading_distance =
        std::max(m_wheel_base, 1.0f) * std::abs(heading_error);
    const float target_speed = compute_path_target_speed(
        projection,
        active_end_s,
        slow_down_at_projection_max,
        projection_max_target_speed_abs
    );
    const float speed_error = state.m_speed - target_speed;
    const float speed_recovery_distance =
        speed_error * speed_error /
        (2.0f * std::max(m_max_acceleration, Utils::eps));

    return PathPotentialEvaluation{
        .projection = projection,
        .components = SimulationLossBreakdown{
            .position = m_loss_weights.position * projection.dist,
            .heading = m_loss_weights.heading * heading_distance,
            .speed = m_loss_weights.speed * speed_recovery_distance,
            .progress = m_loss_weights.progress_tracking * remaining_s,
            .steering = m_loss_weights.steering * (
                state.m_steering_angle * state.m_steering_angle +
                kSteeringVelocityLossFactor *
                    state.m_steering_angle_velocity *
                    state.m_steering_angle_velocity
            ),
            .control = compute_potential_control_regularization(
                state,
                speed_acceleration,
                steer_acceleration
            )
        }
    };
}

float Vehicle::evaluate_path_potential(
    const Vehicle::VehicleTransformState& state,
    const std::vector<VehiclePathPoint>& path,
    const Vehicle::PathArcLengthTable& path_arc_lengths,
    float active_segment_min_s,
    float active_segment_max_s,
    float speed_acceleration,
    float steer_acceleration) const
{
    const PointProjection projection = find_path_projection(
        state,
        path,
        path_arc_lengths,
        active_segment_min_s,
        active_segment_max_s
    );

    const PointProjection vehicle_projection = find_path_projection(
        m_vehicle_state,
        path,
        path_arc_lengths,
        active_segment_min_s,
        active_segment_max_s
    );

    const float total_segment_s = active_segment_max_s - active_segment_min_s;
    const float projection_segment_s = projection.s - active_segment_min_s;
    const float t = projection_segment_s / total_segment_s;

    const double norm_d = projection.dist / total_segment_s;

    const double radius = vehicle_projection.dist;
    // const double der_threshold = 1.0;
    // const double x0 = 1.0f;

    // const double ln_r = log(radius) / log(glm::e<double>());

    // auto grad = [&](double a) {
    //     return 2 * (a * pow(radius, a - 1) - der_threshold) * pow(radius, a - 1) * (1 + a * ln_r);
    // };

    // auto hess = [&](double a) {
    //     return 2 * (
    //         pow(radius, 2 * a - 2) * pow(1 + a * ln_r, 2) +
    //         (a * pow(radius, a - 1) - der_threshold) * pow(radius, a - 1) * (2 * ln_r + a * ln_r * ln_r)
    //     );
    // };

    // const double a = Utils::newton_minimize(grad, hess, x0);
    // const double a = log(der_threshold) / log(radius);

    //

    const double groove_k = 1.0;
    const double plane_k = 0.025;
    const double loss = projection.dist <= radius ?
        groove_k * (1.0 - cos(projection.dist * glm::pi<double>() / radius)) / 2.0 : 
        plane_k * (projection.dist - radius) + groove_k;

    return loss;
}

Vehicle::SimulationLossCoefficients Vehicle::compute_simulation_loss_coefficients(
    const VehicleTransformState& state,
    const PointProjection& projection,
    const PointProjection& target_projection,
    const PathArcLengthTable& path_arc_lengths,
    float total_polyline_length,
    float progress_reference_s,
    float current_next_direction_change_s,
    float speed_acceleration,
    float steer_acceleration) const
{
    return SimulationLossCoefficients{
        .position = compute_position_loss_coefficient(state, target_projection),
        .heading = compute_heading_loss_coefficient(state, target_projection),
        .speed = compute_speed_loss_coefficient(
            state,
            projection,
            path_arc_lengths,
            total_polyline_length,
            current_next_direction_change_s
        ),
        .progress = compute_progress_loss_coefficient(state, projection, target_projection, progress_reference_s),
        .steering = compute_steering_loss_coefficient(state, target_projection),
        .control = compute_control_loss_coefficient(state, speed_acceleration, steer_acceleration)
    };
}

float Vehicle::compute_simulation_loss(
    VehicleTransformState& state,
    const std::vector<VehiclePathPoint>& path,
    const PathArcLengthTable& path_arc_lengths,
    float total_polyline_length,
    float speed_acceleration,
    float steer_acceleration,
    float simulation_time, 
    float dt,
    bool debug,
    float initial_projection_min_s,
    float initial_projection_max_s,
    bool slow_down_at_projection_max,
    float projection_max_target_speed_abs,
    std::vector<glm::vec2>* trajectory,
    SimulationControlCandidateDebug* candidate_debug) const
{
    if (debug) {
        logger().check(simulation_time > 0.0f, "simulation_time must be greater than zero");
        logger().check(path.size() >= 2, "path must contain at least two points");
        check_simulation_step(speed_acceleration, steer_acceleration, dt);
    }

    logger().check(total_polyline_length > Utils::eps, "path length must be greater than zero");

    const glm::vec2 start_position = state.m_position;
    float trajectory_length = 0.0f;
    glm::vec2 previous_trajectory_position = state.m_position;

    if (trajectory) {
        trajectory->clear();
        trajectory->reserve(static_cast<size_t>(std::ceil(simulation_time / dt)) + 1u);
        trajectory->push_back(state.m_position);
    }

    const PathPotentialEvaluation start_potential = evaluate_path_potential(
        state,
        path,
        path_arc_lengths,
        initial_projection_min_s,
        initial_projection_max_s,
        slow_down_at_projection_max,
        projection_max_target_speed_abs,
        speed_acceleration,
        steer_acceleration
    );
    const PointProjection start_projection = start_potential.projection;
    const float progress_reference_dist = start_projection.dist;
    const float reference_initial_path_speed = std::max(
        0.0f,
        glm::dot(forward_vector(state) * state.m_speed, start_projection.tangent)
    );
    const float progress_reference_s = start_projection.s;

    for (float time = 0.0f; time < simulation_time;) {
        const float step_dt = std::min(dt, simulation_time - time);
        vehicle_simulation_step(state, speed_acceleration, steer_acceleration, step_dt, false);
        trajectory_length += glm::length(state.m_position - previous_trajectory_position);
        previous_trajectory_position = state.m_position;
        if (trajectory) {
            trajectory->push_back(state.m_position);
        }
        time += step_dt;
    }

    const PathPotentialEvaluation end_potential = evaluate_path_potential(
        state,
        path,
        path_arc_lengths,
        initial_projection_min_s,
        initial_projection_max_s,
        slow_down_at_projection_max,
        projection_max_target_speed_abs,
        speed_acceleration,
        steer_acceleration
    );
    const PointProjection last_projection = end_potential.projection;
    const PointProjection last_target_projection =
        sample_polyline_at_s(path, path_arc_lengths, last_projection.s);
    const SimulationLossBreakdown loss_breakdown = end_potential.components;
    const float loss = end_potential.total();

    if (candidate_debug) {
        candidate_debug->start_s = progress_reference_s;
        candidate_debug->end_s = last_projection.s;
        candidate_debug->target_end_s = last_target_projection.s;
        candidate_debug->start_dist = progress_reference_dist;
        candidate_debug->end_dist = last_projection.dist;
        candidate_debug->target_end_dist = glm::length(state.m_position - last_target_projection.point);
        candidate_debug->reference_initial_path_speed = reference_initial_path_speed;
        candidate_debug->trajectory_length = trajectory_length;
        candidate_debug->loss_breakdown = loss_breakdown;
        candidate_debug->start_position = start_position;
        candidate_debug->end_position = state.m_position;
        candidate_debug->target_end_point = last_target_projection.point;
    }

    return loss;
}

float Vehicle::compute_simulation_loss_test(
    VehicleTransformState& state,
    const std::vector<VehiclePathPoint>& path,
    const PathArcLengthTable& path_arc_lengths,
    float total_polyline_length,
    float speed_acceleration,
    float steer_acceleration,
    float simulation_time, 
    float dt,
    bool debug,
    float initial_projection_min_s,
    float initial_projection_max_s,
    bool slow_down_at_projection_max,
    float projection_max_target_speed_abs,
    std::vector<glm::vec2>* trajectory,
    SimulationControlCandidateDebug* candidate_debug) const
{
    LOG_METHOD();

    if (debug) {
        logger().check(simulation_time > 0.0f, "simulation_time must be greater than zero");
        logger().check(path.size() >= 2, "path must contain at least two points");
        check_simulation_step(speed_acceleration, steer_acceleration, dt);
    }

    logger().check(total_polyline_length > Utils::eps, "path length must be greater than zero");    

    if (trajectory) {
        trajectory->clear();
        trajectory->reserve(static_cast<size_t>(std::ceil(simulation_time / dt)) + 1u);
        trajectory->push_back(state.m_position);
    }

    float start_potential = evaluate_path_potential(
        state,
        path,
        path_arc_lengths,
        initial_projection_min_s,
        initial_projection_max_s,
        speed_acceleration,
        steer_acceleration
    );

    float trajectory_length = 0.0f;
    glm::vec2 previous_trajectory_position = state.m_position;

    for (float time = 0.0f; time < simulation_time;) {
        const float step_dt = std::min(dt, simulation_time - time);
        vehicle_simulation_step(state, speed_acceleration, steer_acceleration, step_dt, false);
        trajectory_length += glm::length(state.m_position - previous_trajectory_position);
        previous_trajectory_position = state.m_position;
        if (trajectory) {
            trajectory->push_back(state.m_position);
        }
        time += step_dt;
    }

    float end_potential = evaluate_path_potential(
        state,
        path,
        path_arc_lengths,
        initial_projection_min_s,
        initial_projection_max_s,
        speed_acceleration,
        steer_acceleration
    );

    return end_potential - start_potential;
}
