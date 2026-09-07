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
#include "blocks/global_planner_block.h"
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

    struct Desc {
        SensorRecieverBlock::Desc sensors;
        PointMapBlock::Desc point_map;
        GlobalPlannerBlock::Desc global_planner;
    };

    NewCeleris(
        VulkanEngine& engine,
        ManagerBundle& manager_bundle, 
        VulkanQueue& compute_queue,
        VulkanSubmitContext& submit_context,
        VoxelGrid& voxel_grid,
        const VehicleGeometry& vehicle_geometry,
        const Desc& desc
    );

    void start(VulkanSubmitContext&& planner_submit_context);
    void update(VulkanSubmitContext& submit_context);

    void update_global_planner_start_position();
    
    OdometryEstimator& odometry_estimator();
    bool has_lidar_transform();
    Transform* lidar_tranform();
    VoxelGrid* voxel_grid();
    const PathPlanner::PathPlannerResult& global_path_snapshot() const noexcept;
    GlobalPlannerBlock& global_planner() noexcept;
    SensorRecieverBlock& sensor_receiver_block() noexcept;

private:
    VulkanEngine* m_engine = nullptr;
    VoxelGrid* m_voxel_grid = nullptr;
    
    Desc m_desc;
    VehicleGeometry m_vehicle_geometry;

    SensorRecieverBlock m_sensor_reciever_block;
    PointMapBlock m_point_map_block;
    GlobalPlannerBlock m_global_planner_block;
};
