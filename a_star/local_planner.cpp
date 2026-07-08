#include "local_planner.h"

#include "vehicle.h"
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
    constexpr float kPathSegmentMinLength = 1e-3f;
    constexpr float kSelfIntersectionMinSSeparation = 2.0f;
    constexpr float kSegmentTransitionEps = 0.05f;

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

    float cross2(glm::vec2 a, glm::vec2 b)
    {
        return a.x * b.y - a.y * b.x;
    }

    void add_breakpoint(std::vector<float>& breakpoints, float s, float path_length)
    {
        if (!std::isfinite(s))
            return;

        breakpoints.push_back(std::clamp(s, 0.0f, path_length));
    }

    bool segment_intersection_parameters(
        glm::vec2 a,
        glm::vec2 b,
        glm::vec2 c,
        glm::vec2 d,
        float& t,
        float& u)
    {
        const glm::vec2 r = b - a;
        const glm::vec2 s = d - c;
        const float denominator = cross2(r, s);
        if (std::abs(denominator) <= 1e-6f)
            return false;

        const glm::vec2 c_minus_a = c - a;
        t = cross2(c_minus_a, s) / denominator;
        u = cross2(c_minus_a, r) / denominator;
        return
            t >= -1e-5f && t <= 1.0f + 1e-5f &&
            u >= -1e-5f && u <= 1.0f + 1e-5f;
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

size_t LocalPlanner::active_path_segment_index() const noexcept {
    return m_active_path_segment;
}

size_t LocalPlanner::path_segment_count() const noexcept {
    return m_path_segments.size();
}

uint64_t LocalPlanner::path_generation() const noexcept {
    return m_path_generation;
}

const std::vector<VehiclePathPoint>& LocalPlanner::vehicle_path() const noexcept {
    return m_global_astar_path;
}

const Vehicle::PathArcLengthTable& LocalPlanner::vehicle_path_arc_lengths() const noexcept {
    return m_global_astar_path_arc_lengths;
}

void LocalPlanner::reset_tracking()
{
    reset_tracking_state();
}

void LocalPlanner::reset_tracking_state()
{
    m_path_progress_s = 0.0f;
    m_path_window_min_s = 0.0f;
    m_path_window_max_s = 0.0f;
    m_path_progress_floor_s = 0.0f;
    m_active_path_segment = 0;
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
    rebuild_path_segments();

    if (reset_tracking)
        reset_tracking_state();
}

void LocalPlanner::rebuild_path_segments()
{
    m_path_segments.clear();
    if (m_global_astar_path.size() < 2 || m_path_length <= Utils::eps)
        return;

    std::vector<float> breakpoints;
    breakpoints.reserve(m_global_astar_path.size() + m_global_astar_path_arc_lengths.direction_change_s.size() + 2u);
    add_breakpoint(breakpoints, 0.0f, m_path_length);
    add_breakpoint(breakpoints, m_path_length, m_path_length);

    for (float direction_change_s : m_global_astar_path_arc_lengths.direction_change_s) {
        add_breakpoint(breakpoints, direction_change_s, m_path_length);
    }

    const std::vector<float>& point_s = m_global_astar_path_arc_lengths.point_s;

    for (size_t i = 1; i < m_global_astar_path.size(); i++) {
        const glm::vec2 a = m_global_astar_path[i - 1].position;
        const glm::vec2 b = m_global_astar_path[i].position;
        const float length_a = point_s[i] - point_s[i - 1];
        if (length_a <= Utils::eps)
            continue;

        for (size_t j = i + 2; j < m_global_astar_path.size(); j++) {
            const glm::vec2 c = m_global_astar_path[j - 1].position;
            const glm::vec2 d = m_global_astar_path[j].position;
            const float length_b = point_s[j] - point_s[j - 1];
            if (length_b <= Utils::eps)
                continue;

            float t = 0.0f;
            float u = 0.0f;
            if (!segment_intersection_parameters(a, b, c, d, t, u))
                continue;

            const float s_a = point_s[i - 1] + std::clamp(t, 0.0f, 1.0f) * length_a;
            const float s_b = point_s[j - 1] + std::clamp(u, 0.0f, 1.0f) * length_b;
            if (std::abs(s_a - s_b) < kSelfIntersectionMinSSeparation)
                continue;

            add_breakpoint(breakpoints, s_a, m_path_length);
            add_breakpoint(breakpoints, s_b, m_path_length);
        }
    }

    std::sort(breakpoints.begin(), breakpoints.end());
    breakpoints.erase(
        std::unique(
            breakpoints.begin(),
            breakpoints.end(),
            [](float a, float b) {
                return std::abs(a - b) <= kPathSegmentMinLength;
            }
        ),
        breakpoints.end()
    );

    if (breakpoints.size() < 2) {
        m_path_segments.push_back(PathSegment{
            .s_begin = 0.0f,
            .s_end = m_path_length,
            .dir = Vehicle::sample_polyline_at_s(
                m_global_astar_path,
                m_global_astar_path_arc_lengths,
                0.0f
            ).dir
        });
        return;
    }

    for (size_t i = 1; i < breakpoints.size(); i++) {
        const float s_begin = breakpoints[i - 1];
        const float s_end = breakpoints[i];
        if (s_end - s_begin <= kPathSegmentMinLength)
            continue;

        const float mid_s = 0.5f * (s_begin + s_end);
        const Vehicle::PointProjection mid_point =
            Vehicle::sample_polyline_at_s(
                m_global_astar_path,
                m_global_astar_path_arc_lengths,
                mid_s
            );
        m_path_segments.push_back(PathSegment{
            .s_begin = s_begin,
            .s_end = s_end,
            .dir = mid_point.dir
        });
    }
}

const LocalPlanner::PathSegment* LocalPlanner::active_path_segment() const noexcept
{
    if (m_path_segments.empty())
        return nullptr;

    const size_t clamped_index =
        std::min(m_active_path_segment, m_path_segments.size() - 1u);
    return &m_path_segments[clamped_index];
}

bool LocalPlanner::active_path_segment_has_next() const noexcept
{
    return m_active_path_segment + 1u < m_path_segments.size();
}

bool LocalPlanner::active_path_segment_ends_with_direction_switch() const noexcept
{
    if (!active_path_segment_has_next())
        return false;

    const PathSegment& current = m_path_segments[m_active_path_segment];
    const PathSegment& next = m_path_segments[m_active_path_segment + 1u];
    return current.dir != next.dir;
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

    if (m_path_segments.empty()) {
        rebuild_path_segments();
    }

    if (m_path_segments.empty()) {
        m_last_simulation_candidates.clear();
        last_control_command = Vehicle::VehicleControlCommand{};
        m_last_applied_command = AppliedVehicleCommand{};
        m_path_window_min_s = 0.0f;
        m_path_window_max_s = 0.0f;
        m_path_progress_floor_s = 0.0f;
        return VehicleCommand{};
    }

    if (m_active_path_segment >= m_path_segments.size()) {
        m_active_path_segment = m_path_segments.size() - 1u;
    }

    auto project_on_active_segment = [&]() {
        const PathSegment& segment = m_path_segments[m_active_path_segment];
        return vehicle.find_path_projection(
            vehicle.state(),
            m_global_astar_path,
            m_global_astar_path_arc_lengths,
            segment.s_begin,
            segment.s_end
        );
    };

    Vehicle::PointProjection current_projection = project_on_active_segment();
    for (;;) {
        if (!active_path_segment_has_next())
            break;

        const PathSegment& segment = m_path_segments[m_active_path_segment];
        const float segment_switch_radius =
            std::max(kSegmentTransitionEps, follow_params.segment_switch_radius);
        const Vehicle::PointProjection segment_end_projection =
            Vehicle::sample_polyline_at_s(
                m_global_astar_path,
                m_global_astar_path_arc_lengths,
                segment.s_end
            );
        const float segment_end_dist =
            glm::length(vehicle.state().m_position - segment_end_projection.point);

        if (segment_end_dist > segment_switch_radius)
            break;

        m_active_path_segment++;
        m_path_progress_floor_s = m_path_segments[m_active_path_segment].s_begin;
        current_projection = project_on_active_segment();
    }

    m_path_progress_s = current_projection.s;
    m_has_path_progress = true;

    const PathSegment* active_segment = active_path_segment();
    if (!active_segment) {
        m_last_simulation_candidates.clear();
        last_control_command = Vehicle::VehicleControlCommand{};
        m_last_applied_command = AppliedVehicleCommand{};
        return VehicleCommand{};
    }

    const float candidate_projection_min_s = active_segment->s_begin;
    const float candidate_projection_max_s = active_segment->s_end;
    m_path_window_min_s = candidate_projection_min_s;
    m_path_window_max_s = candidate_projection_max_s;

    Vehicle::SimulationControlSearchDesc search_desc;
    search_desc.max_results =
        search_desc.speed_acceleration_samples *
        search_desc.steer_acceleration_samples;
    search_desc.initial_projection_min_s = candidate_projection_min_s;
    search_desc.initial_projection_max_s = candidate_projection_max_s;
    search_desc.slow_down_at_projection_max =
        !active_path_segment_has_next() ||
        active_path_segment_ends_with_direction_switch();
    search_desc.projection_max_target_speed_abs =
        active_path_segment_has_next()
            ? std::max(0.0f, follow_params.direction_switch_arrival_speed)
            : 0.0f;

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
