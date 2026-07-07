#pragma once

#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "../vulkan_self/logger/logger_header.h"
#include "a_star_structures.h"
#include "pure_pursuit_vehicle.h"
#include "vehichle.h"

using PurePursuitClock = std::chrono::high_resolution_clock;

class PathIntersectionDetector;
class VulkanSubmitContext;
class VehicleCommand;

class PurePursuitLocalPlanner {
public:
    _XCLASS_NAME(PurePursuitLocalPlanner);

public:
    PurePursuitLocalPlanner(float step_dt_min = 0.001f, float step_dt_max = 0.05f);
    ~PurePursuitLocalPlanner() noexcept = default;

    PurePursuitLocalPlanner(const PurePursuitLocalPlanner&) = delete;
    PurePursuitLocalPlanner& operator=(const PurePursuitLocalPlanner&) = delete;

    PurePursuitLocalPlanner(PurePursuitLocalPlanner&&) noexcept = default;
    PurePursuitLocalPlanner& operator=(PurePursuitLocalPlanner&&) noexcept = default;

    void update_timestamp();
    float calculate_delta_time();
    void predict_vehicle_state(PurePursuitVehicle& vehicle);
    void reset_tracking();

    void set_astar_path(const std::vector<NonholonomicPos>& astar_path);
    void set_astar_path(const std::vector<NonholonomicPos>& astar_path, uint64_t generation);
    VehicleCommand predict_vehicle_command(
        const PurePursuitVehicle& vehicle,
        PathIntersectionDetector& intersection_detector,
        VulkanSubmitContext& submit_context
    );

    VehicleCommand step(
        const PurePursuitVehicle& vehicle,
        PathIntersectionDetector& intersection_detector,
        VulkanSubmitContext& submit_context,
        const std::vector<NonholonomicPos>* astar_path = nullptr
    );

    const PurePursuitVehicle::PurePursuitStepResult& last_step_result() const noexcept;
    float path_progress_s() const noexcept;
    float path_length() const noexcept;
    float path_window_min_s() const noexcept;
    float path_window_max_s() const noexcept;
    uint64_t path_generation() const noexcept;

private:
    std::vector<VehiclePathPoint> m_global_astar_path;
    Vehicle::PathArcLengthTable m_global_astar_path_arc_lengths;
    std::optional<PurePursuitClock::time_point> m_previous_timestamp = std::nullopt;
    std::optional<PurePursuitClock::time_point> m_current_timestamp = std::nullopt;
    std::optional<PurePursuitClock::time_point> m_previous_command_timestamp = std::nullopt;
    std::optional<PurePursuitVehicle::PurePursuitControlCommand> m_last_control_command = std::nullopt;
    PurePursuitVehicle::PurePursuitStepResult m_last_step_result;
    float m_path_progress_s = 0.0f;
    float m_path_length = 0.0f;
    float m_path_window_min_s = 0.0f;
    float m_path_window_max_s = 0.0f;
    float m_path_progress_floor_s = 0.0f;
    uint64_t m_path_generation = 0;
    bool m_has_path_progress = false;
    float m_step_dt_min = std::numeric_limits<float>::min();
    float m_step_dt_max = std::numeric_limits<float>::max();

    void set_vehicle_path(std::vector<VehiclePathPoint> path, bool reset_tracking);
    void reset_tracking_state();
    float calculate_command_delta_time();
};
