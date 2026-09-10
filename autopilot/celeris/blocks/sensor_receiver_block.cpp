#include "sensor_receiver_block.h"

#include "../../../voxel_grid_vulkan/voxel_grid_structures.h"
#include "../../../voxel_grid_vulkan/voxel_grid.h"
#include "../../sensors/lidar/lidar_scan.h"
#include "../../../renderer/transform.h"
#include "point_map_block.h"

SensorReceiverBlock::SensorReceiverBlock(
    VulkanEngine& engine,
    ManagerBundle& manager_bundle,
    VulkanQueue& compute_queue,
    const Desc& desc)
    :   m_engine(&engine),
        m_desc(desc),
        m_point_cloud_preprocessor(
            engine.device(), 
            compute_queue, 
            manager_bundle.compute_pass_manager()
        ),
        m_lidar_scan_receiver(
            manager_bundle,
            m_point_cloud_preprocessor,
            desc.lidar_port,
            desc.lidar_queue_capacity),
        m_imu_receiver(desc.imu_port, desc.imu_queue_capacity),
        m_deskewer(m_odometry_estimator),
        m_voxel_write_list(VulkanBuffer::create_host_visible_storage_buffer(
            engine, 
            sizeof(uint32_t) * 4 + sizeof(VoxelWriteGPU) * desc.max_voxel_writes)) {
}

void SensorReceiverBlock::start_imu_receiver() {
    m_imu_receiver.start();
}

void SensorReceiverBlock::start_lidar_scan_receiver() {
    m_lidar_scan_receiver.start();
}

void SensorReceiverBlock::start() {
    start_imu_receiver();
    start_lidar_scan_receiver();
}

void SensorReceiverBlock::update(PointMapBlock& point_map_block, VoxelGrid* voxel_grid) {
    try_receive_and_process_imu();
    if (!m_odometry_estimator.is_gravity_calibration_underway())
        try_receive_and_process_lidar_scan(point_map_block, voxel_grid);
}

OdometryEstimator& SensorReceiverBlock::odometry_estimator() {
    return m_odometry_estimator;
}

bool SensorReceiverBlock::has_lidar_transform() {
    return m_network_scan.get();
}

Transform* SensorReceiverBlock::lidar_tranform() {
    if (!m_network_scan)
        return nullptr;
    return &m_network_scan->point_cloud().transform;
}

void SensorReceiverBlock::voxelize(
    VoxelGrid* voxel_grid, 
    std::unique_ptr<LidarScan>& lidar_scan) 
{
    logger().check(voxel_grid, "Voxel grid was null");

    voxel_grid->voxelize_point_cloud(
        *m_engine,
        lidar_scan->point_cloud(),
        lidar_scan->normal_buffer(),
        m_voxel_write_list,
        m_desc.max_voxel_writes
    );
}

void SensorReceiverBlock::try_receive_and_process_imu() {
    LOG_METHOD();

    ImuMeasurement imu_message{};
    if (m_imu_receiver.try_pop_back_imu_message(imu_message)) {
        m_odometry_estimator.submit_imu(imu_message);

        // Odometry latest_odometry = m_odometry_estimator.get_latest_odometry();

        // m_lidar_transform.position = latest_odometry.position;
        // m_lidar_transform.rotation = latest_odometry.orientation;
    }
}

void SensorReceiverBlock::try_receive_and_process_lidar_scan(PointMapBlock& point_map_block, VoxelGrid* voxel_grid) {
    LOG_METHOD();

    logger().check(m_engine, "Engine was null");
    // logger().check(voxel_grid, "Voxel grid was null");

    LidarMessage lidar_message;
    if (!m_lidar_scan_receiver.try_pop_front_lidar_msg(lidar_message))
        return;

    Odometry lidar_msg_odometry;
    if (m_odometry_estimator.interpolate_odometry(lidar_message.scan_timestamp, lidar_msg_odometry))
        m_deskewer.deskew(lidar_message, lidar_msg_odometry);
    
    std::unique_ptr<LidarScan> scan = m_lidar_scan_receiver.try_get_lidar_scan_from_lidar_msg(lidar_message);
    if (!scan)
        return;

    if (m_network_scan)
        m_retired_network_scans.push_back(std::move(m_network_scan));
    m_network_scan = std::move(scan);
    
    while (m_retired_network_scans.size() > m_engine->num_frames_in_flight())
        m_retired_network_scans.pop_front();

    Odometry closest_prev_odometry{};
    if (!m_odometry_estimator.get_closest_prev_odometry(m_network_scan->timestamp(), closest_prev_odometry))
        closest_prev_odometry.timestamp_ns = m_network_scan->timestamp();

    m_network_scan->point_cloud().transform.position = closest_prev_odometry.position;
    m_network_scan->point_cloud().transform.rotation = closest_prev_odometry.orientation;

    point_map_block.fit(m_network_scan);

    Odometry last_lidar_odometry{};
    if (m_odometry_estimator.get_last_lidar_odometry(last_lidar_odometry))
        last_lidar_odometry.timestamp_ns = m_network_scan->timestamp();

    m_odometry_estimator.submit_lidar_imu_fusion(
        *m_network_scan, 
        closest_prev_odometry, 
        last_lidar_odometry
    );

    point_map_block.insert(m_network_scan);

    voxelize(voxel_grid, m_network_scan);

    m_received_scan_count++;
}
