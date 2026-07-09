#pragma once

#include <cstdint>
#include <vector>

#include "a_star_structures.h"
#include "vehicle_base.h"

class PathIntersectionDetector;
class VulkanSubmitContext;
class VehicleCommand;

class LocalPlannerBase {
public:
    virtual ~LocalPlannerBase() noexcept = default;

    LocalPlannerBase(const LocalPlannerBase&) = delete;
    LocalPlannerBase& operator=(const LocalPlannerBase&) = delete;

    LocalPlannerBase(LocalPlannerBase&&) noexcept = default;
    LocalPlannerBase& operator=(LocalPlannerBase&&) noexcept = default;

    virtual void update_timestamp() = 0;
    virtual float calculate_delta_time() = 0;
    virtual void predict_vehicle_state(VehicleBase& vehicle) = 0;
    virtual void reset_tracking() = 0;

    virtual void set_astar_path(const std::vector<NonholonomicPos>& astar_path) = 0;
    virtual void set_astar_path(
        const std::vector<NonholonomicPos>& astar_path,
        uint64_t generation
    ) = 0;

    virtual VehicleCommand predict_vehicle_command(
        const VehicleBase& vehicle,
        PathIntersectionDetector& intersection_detector,
        VulkanSubmitContext& submit_context
    ) = 0;

    virtual VehicleCommand step(
        const VehicleBase& vehicle,
        PathIntersectionDetector& intersection_detector,
        VulkanSubmitContext& submit_context,
        const std::vector<NonholonomicPos>* astar_path = nullptr
    ) = 0;

    virtual float path_progress_s() const noexcept = 0;
    virtual float path_length() const noexcept = 0;
    virtual float path_window_min_s() const noexcept = 0;
    virtual float path_window_max_s() const noexcept = 0;
    virtual uint64_t path_generation() const noexcept = 0;
    virtual const std::vector<VehiclePathPoint>& vehicle_path() const noexcept = 0;
    virtual const VehicleBase::PathArcLengthTable& vehicle_path_arc_lengths() const noexcept = 0;

protected:
    LocalPlannerBase() noexcept = default;
};
