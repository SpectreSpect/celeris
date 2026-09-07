#pragma once

#include <cstdint>
#include <cstddef>

#include "../../renderer/point_cloud/gicp/voxel_map_point_inserter.h"
#include "../../renderer/point_cloud/gicp/voxel_map_point_reseter.h"
#include "../../renderer/point_cloud/point_cloud_preprocessor.h"
#include "../../renderer/point_cloud/gicp/voxel_point_map.h"
#include "../sensors/lidar/deskewing/lidar_scan_deskewer.h"
#include "../../renderer/point_cloud/gicp/gicp_pass.h"
#include "../../a_star/path_intersection_detector.h"
#include "../../vulkan_self/logger/logger_header.h"
#include "../collision/collision_escape_resolver.h"
#include "../sensors/lidar/lidar_scan_receiver.h"
#include "../odometry/odometry_estimator.h"
#include "blocks/sensor_reciever_block.h"
#include "../sensors/imu/imu_receiver.h"
#include "blocks/point_map_block.h"
#include "../path_planner.h"

class VulkanSubmitContext;
class ManagerBundle;
class VulkanEngine;
class VulkanQueue;
class VoxelGrid;
class Transform;
class Camera;

class NewCeleris {
public:
    _XCLASS_NAME(NewCeleris);

    struct CelerisDesc {
        SensorRecieverBlock::SensorRecieverBlockDesc sensor_reciever_block_desc;
        PointMapBlock::PointMapBlockDesc point_map_block_desc;

        uint32_t path_intersection_detector_max_path_points = 1024;
        PathPlanner::PathPlannerDesc path_planner_desc{};
        CollisionEscapeResolver::CollisionEscapeResolverDesc
            collision_escape_resolver_desc{};
    };

    NewCeleris(
        VulkanEngine& engine,
        ManagerBundle& manager_bundle, 
        VulkanQueue& compute_queue,
        VulkanSubmitContext& submit_context,
        VoxelGrid& voxel_grid,
        const VehicleGeometry& vehicle_geometry,
        const CelerisDesc& desc
    );

    void start(VulkanSubmitContext&& planner_submit_context);
    void update(VulkanSubmitContext& submit_context);

    void set_start(const NonholonomicPos& position);
    void set_goal(const NonholonomicPos& position);

    bool adjust_to_ground(
        glm::vec3& output,
        int max_step_up = 500,
        int max_drop = 500,
        int max_y_diff = -1,
        bool allow_flying_over_precepices = true
    );

    void request_path_replan();
    void update_start_position();
    
    OdometryEstimator& odometry_estimator();
    bool has_lidar_transform();
    Transform* lidar_tranform();
    VoxelGrid* voxel_grid();
    glm::vec3 voxel_center_bottom_world_pos(const glm::ivec3& voxel_pos);
    glm::vec3 voxel_center_world_pos(const glm::ivec3& voxel_pos);
    NonholonomicPos start_position() const noexcept;
    NonholonomicPos goal_position() const noexcept;
    const PathPlanner::PathPlannerResult& path_planner_snapshot() const noexcept;

private:
    VulkanEngine* m_engine = nullptr;
    VoxelGrid* m_voxel_grid = nullptr;
    
    CelerisDesc m_desc;
    VehicleGeometry m_vehicle_geometry;

    SensorRecieverBlock m_sensor_reciever_block;
    PointMapBlock m_point_map_block;

    PathIntersectionDetector m_path_intersection_detector;
    PathPlanner m_path_planner;
    CollisionEscapeResolver m_collision_escape_resolver;
    PathPlanner::PathPlannerResult m_path_planner_snapshot{};

    NonholonomicPos m_start_position{};
    NonholonomicPos m_goal_position{};
    
    bool path_replan_required(VulkanSubmitContext& submit_context);
};
