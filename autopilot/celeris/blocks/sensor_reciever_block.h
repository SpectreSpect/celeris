#pragma once

#include <cstddef>
#include <cstdint>

#include "../../../renderer/point_cloud/point_cloud_preprocessor.h"
#include "../../sensors/lidar/deskewing/lidar_scan_deskewer.h"
#include "../../../vulkan_self/logger/logger_header.h"
#include "../../sensors/lidar/lidar_scan_receiver.h"
#include "../../../vulkan_self/vulkan_buffer.h"
#include "../../odometry/odometry_estimator.h"
#include "../../sensors/imu/imu_receiver.h"

class PointCloudPreprocessor;
class ManagerBundle;
class PointMapBlock;
class VulkanEngine;
class VulkanQueue;
class Transform;
class VoxelGrid;

class SensorRecieverBlock {
public:
    _XCLASS_NAME(SensorRecieverBlock);

    struct SensorRecieverBlockDesc {
        uint16_t lidar_scan_receiver_port = 5000;
        size_t lidar_scan_receiver_max_queued_messages = 3;

        uint16_t imu_receiver_port = 5003;
        size_t max_queued_imu_messages = 1;

        uint32_t max_voxel_grid_write_count = 100000;
    };

    SensorRecieverBlock(
        VulkanEngine& engine,
        ManagerBundle& manager_bundle,
        VulkanQueue& compute_queue,
        const SensorRecieverBlockDesc& desc
    );

    void start();
    
    void start_imu_reciever();
    void start_lidar_scan_receiver();
    
    void update(PointMapBlock& point_map_block, VoxelGrid* voxel_grid);

    OdometryEstimator& odometry_estimator();
    bool has_lidar_transform();
    Transform* lidar_tranform();

private:
    VulkanEngine* m_engine = nullptr;

    SensorRecieverBlockDesc m_desc;

    LidarScanReceiver m_lidar_scan_receiver;
    ImuReceiver m_imu_receiver;
    PointCloudPreprocessor m_point_cloud_preprocessor;
    OdometryEstimator m_odometry_estimator;
    LidarScanDeskewer m_deskewer;

    std::unique_ptr<LidarScan> m_network_scan;
    std::deque<std::unique_ptr<LidarScan>> m_retired_network_scans;
    uint32_t m_received_scan_count = 0;

    VulkanBuffer m_voxel_write_list;

    void voxelize(VoxelGrid* voxel_grid, std::unique_ptr<LidarScan>& lidar_scan);

    void try_receive_and_process_imu();
    void try_receive_and_process_lidar_scan(PointMapBlock& point_map_block, VoxelGrid* voxel_grid);
};
