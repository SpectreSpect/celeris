#pragma once

#include <cstdint>
#include <cstddef>

#include "../renderer/point_cloud/gicp/voxel_map_point_inserter.h"
#include "../renderer/point_cloud/gicp/voxel_map_point_reseter.h"
#include "../renderer/point_cloud/point_cloud_preprocessor.h"
#include "../renderer/point_cloud/gicp/voxel_point_map.h"
#include "sensors/lidar/deskewing/lidar_scan_deskewer.h"
#include "../renderer/point_cloud/gicp/gicp_pass.h"
#include "../a_star/path_intersection_detector.h"
#include "../vulkan_self/logger/logger_header.h"
#include "sensors/lidar/lidar_scan_receiver.h"
#include "odometry/odometry_estimator.h"
#include "sensors/imu/imu_receiver.h"
#include "path_planner.h"

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
        uint16_t lidar_scan_receiver_port = 5000;
        size_t lidar_scan_receiver_max_queued_messages = 3;

        uint16_t imu_receiver_port = 5003;
        size_t max_queued_imu_messages = 1;

        uint32_t voxel_point_map_num_hash_table_slots = 1500000;
        uint32_t voxel_point_map_max_map_point_count = 1500000;
        uint32_t max_gicp_iterations = 10;

        uint32_t max_write_count = 100000;

        uint32_t path_intersection_detector_max_path_points = 1024;
        PathPlanner::PathPlannerDesc path_planner_desc{};
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
    void update();

    void set_start(const NonholonomicPos& position);
    void set_start(const Camera& camera);
    void set_goal(const NonholonomicPos& position);

    bool adjust_to_ground(
        glm::vec3& output,
        int max_step_up = 500,
        int max_drop = 500,
        int max_y_diff = -1,
        bool allow_flying_over_precepices = true
    );

    // m_path_planner.request_path_replan(start, goal);
    void request_path_replan();
    
    OdometryEstimator& odometry_estimator();
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

    PointCloudPreprocessor m_point_cloud_preprocessor;

    LidarScanReceiver m_lidar_scan_receiver;
    ImuReceiver m_imu_receiver;
    OdometryEstimator m_odometry_estimator;
    LidarScanDeskewer m_deskewer;

    VoxelPointMap m_voxel_point_map;
    VoxelMapPointInserter m_voxel_map_inserter;
    VoxelMapPointReseter m_voxel_map_reseter;
    GICPPass m_gicp_pass;

    VulkanBuffer m_voxel_write_list;

    std::unique_ptr<LidarScan> m_network_scan;
    std::deque<std::unique_ptr<LidarScan>> m_retired_network_scans;
    uint32_t m_received_scan_count = 0;

    PathIntersectionDetector m_path_intersection_detector;
    PathPlanner m_path_planner;
    PathPlanner::PathPlannerResult m_path_planner_snapshot{};

    NonholonomicPos m_start_position{};
    NonholonomicPos m_goal_position{};
    
    void try_receive_and_process_imu();
    void try_receive_and_process_lidar_scan();
};