#pragma once

#include <cstdint>
#include <cstddef>
#include "../renderer/point_cloud/gicp/voxel_map_point_inserter.h"
#include "../renderer/point_cloud/gicp/voxel_map_point_reseter.h"
#include "../renderer/point_cloud/point_cloud_preprocessor.h"
#include "../renderer/point_cloud/gicp/voxel_point_map.h"
#include "sensors/lidar/deskewing/lidar_scan_deskewer.h"
#include "../renderer/point_cloud/gicp/gicp_pass.h"
#include "../vulkan_self/logger/logger_header.h"
#include "sensors/lidar/lidar_scan_receiver.h"

#include "odometry/odometry_estimator.h"
#include "sensors/imu/imu_receiver.h"

class ManagerBundle;
class VulkanEngine;
class VulkanQueue;
class VoxelGrid;
class Transform;

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
    };

    NewCeleris(
        VulkanEngine& engine,
        ManagerBundle& manager_bundle, 
        VulkanQueue& compute_queue,
        VoxelGrid& voxel_grid,
        const CelerisDesc& desc
    );

    void start();
    void update();
    
    OdometryEstimator& odometry_estimator();
    Transform* lidar_tranform();
    VoxelGrid* voxel_grid();

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
    
    void try_receive_and_process_imu();
    void try_receive_and_process_lidar_scan();
};