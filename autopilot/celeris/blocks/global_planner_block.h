#pragma once

#include <cstdint>

#include "../../../a_star/path_intersection_detector.h"
#include "../../collision/collision_escape_resolver.h"
#include "../../../vulkan_self/logger/logger_header.h"
#include "../../path_planner.h"

class VulkanSubmitContext;
class SensorReceiverBlock;
class VehicleGeometry;
class ManagerBundle;
class VulkanEngine;
class VoxelGrid;

class GlobalPlannerBlock {
public:
    _XCLASS_NAME(GlobalPlannerBlock);

    struct Desc {
        uint32_t max_intersection_path_points = 1024;
        PathPlanner::Desc planner{};
        CollisionEscapeResolver::Desc collision_escape{};
    };

    GlobalPlannerBlock(
        VulkanEngine& engine,
        ManagerBundle& manager_bundle,
        VulkanSubmitContext& submit_context,
        VoxelGrid& voxel_grid,
        const VehicleGeometry& vehicle_geometry,
        const Desc& desc
    );

    void start(VulkanSubmitContext&& planner_submit_context);
    void update(
        VulkanSubmitContext& submit_context, 
        SensorReceiverBlock& sensor_receiver_block
    );

    void request_path_replan();
    void update_start_position(SensorReceiverBlock& sensor_receiver_block);

    void set_start(const NonholonomicPos& position);
    void set_goal(const NonholonomicPos& position);

    bool adjust_to_ground(
        glm::vec3& output,
        int max_step_up = 500,
        int max_drop = 500,
        int max_y_diff = -1,
        bool allow_flying_over_precepices = true
    );

    NonholonomicPos start_position() const noexcept;
    NonholonomicPos goal_position() const noexcept;
    const PathPlanner::PathPlannerResult& path_snapshot() const noexcept;

private:
    const VehicleGeometry* m_vehicle_geometry = nullptr;
    VoxelGrid* m_voxel_grid = nullptr;

    PathIntersectionDetector m_path_intersection_detector;
    PathPlanner m_path_planner;
    CollisionEscapeResolver m_collision_escape_resolver;
    PathPlanner::PathPlannerResult m_path_snapshot{};

    NonholonomicPos m_start_position{};
    NonholonomicPos m_goal_position{};

    void sync_path_snapshot();
    bool path_replan_required(
        VulkanSubmitContext& submit_context, 
        SensorReceiverBlock& sensor_receiver_block
    );
};
