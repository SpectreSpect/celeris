#pragma once

#include <vector>
#include <limits>
#include <chrono>
#include <cstdint>
#include <optional>

#include "../vulkan_self/logger/logger_header.h"
#include "a_star_structures.h"
#include "vehichle.h"

using Clock = std::chrono::high_resolution_clock;

class PathIntersectionDetector;
class VulkanSubmitContext;
class VehicleCommand;

class LocalPlanner {
public:
    _XCLASS_NAME(LocalPlanner);

public:
    enum class LocalPlannerMode {
        FOLLOWING_ASTAR,
        FOLLOWING_MINI_REEDS_SHEPP,
        WAITING_FOR_GLOBAL_REPLAN
    };

public:
    LocalPlanner(float step_dt_min = 0.001f, float step_dt_max = 0.05f);
    ~LocalPlanner() noexcept = default;

    LocalPlanner(const LocalPlanner&) = delete;
    LocalPlanner& operator=(const LocalPlanner&) = delete;

    LocalPlanner(LocalPlanner&&) noexcept = default;
    LocalPlanner& operator=(LocalPlanner&&) noexcept = default;

    void update_timestamp();
    float calculate_delta_time();
    void predict_vehicle_state(Vehicle& vehicle);

    void set_astar_path(const std::vector<NonholonomicPos>& astar_path);
    void set_astar_path(const std::vector<NonholonomicPos>& astar_path, uint64_t generation);
    VehicleCommand predict_vehicle_command(
        const Vehicle& vehicle,
        PathIntersectionDetector& intersection_detector,
        VulkanSubmitContext& submit_context
    );

    VehicleCommand step(
        const Vehicle& vehicle,
        PathIntersectionDetector& intersection_detector,
        VulkanSubmitContext& submit_context,
        const std::vector<NonholonomicPos>* astar_path = nullptr
    );

    const std::vector<Vehicle::SimulationControlCandidate>& last_simulation_candidates() const noexcept;
    float path_progress_s() const noexcept;
    float path_length() const noexcept;
    uint64_t path_generation() const noexcept;

private:
    struct AppliedVehicleCommand {
        float speed_acceleration = 0.0f;
        float steering_angle_velocity = 0.0f;
    };

    LocalPlannerMode m_mode = LocalPlannerMode::FOLLOWING_ASTAR;
    std::vector<VehiclePathPoint> m_global_astar_path;
    std::vector<VehiclePathPoint> m_mini_reeds_shepp_path;
    std::optional<Clock::time_point> m_previous_timestamp = std::nullopt;
    std::optional<Clock::time_point> m_current_timestamp = std::nullopt;
    std::optional<Vehicle::VehicleControlCommand> last_control_command = std::nullopt;
    std::optional<AppliedVehicleCommand> m_last_applied_command = std::nullopt;
    std::vector<Vehicle::SimulationControlCandidate> m_last_simulation_candidates;
    float m_path_progress_s = 0.0f;
    float m_path_length = 0.0f;
    uint64_t m_path_generation = 0;
    bool m_has_path_progress = false;
    float m_step_dt_min = std::numeric_limits<float>::min();
    float m_step_dt_max = std::numeric_limits<float>::max();

    void set_vehicle_path(std::vector<VehiclePathPoint> path, bool reset_tracking);
    void reset_tracking_state();
};
