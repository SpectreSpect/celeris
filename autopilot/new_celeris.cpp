#include "new_celeris.h"

#include "../voxel_grid_vulkan/voxel_grid.h"
#include "../vulkan_self/vulkan_engine.h"
#include "../managers/manager_bundle.h"
#include "sensors/lidar/lidar_scan.h"

NewCeleris::NewCeleris(
    VulkanEngine& engine,
    ManagerBundle& manager_bundle, 
    VulkanQueue& compute_queue,
    VoxelGrid& voxel_grid,
    const CelerisDesc& desc)
    :   m_engine(&engine),
        m_voxel_grid(&voxel_grid),
        m_desc(desc),
        m_point_cloud_preprocessor(engine.device(), compute_queue, manager_bundle.compute_pass_manager()),
        m_lidar_scan_receiver(
            manager_bundle, 
            m_point_cloud_preprocessor, 
            desc.lidar_scan_receiver_port,
            desc.lidar_scan_receiver_max_queued_messages),
        m_imu_receiver(desc.imu_receiver_port, desc.max_queued_imu_messages),
        m_deskewer(m_odometry_estimator),
        m_voxel_point_map(
            engine,
            desc.voxel_point_map_num_hash_table_slots,
            desc.voxel_point_map_max_map_point_count),
        m_voxel_map_inserter(engine, manager_bundle.compute_pass_manager()),
        m_voxel_map_reseter(engine, manager_bundle.compute_pass_manager()),
        m_gicp_pass(engine, manager_bundle.compute_pass_manager()),
        m_voxel_write_list(VulkanBuffer::create_host_visible_storage_buffer(
            engine, 
            sizeof(uint32_t) * 4 + sizeof(VoxelWriteGPU) * desc.max_write_count)) {
    LOG_METHOD();
    logger().check(desc.voxel_point_map_num_hash_table_slots > 0, 
                 "The number of voxel point map hash table slots must be greater than 0");
    logger().check(desc.voxel_point_map_max_map_point_count > 0, 
                 "The maximum number of voxel point map points must be greater than 0");
    
    m_voxel_map_reseter.reset(m_voxel_point_map);
}

void NewCeleris::start() {
    LOG_METHOD();

    m_lidar_scan_receiver.start();
    m_imu_receiver.start();
}

void NewCeleris::update() {
    LOG_METHOD();
    
    try_receive_and_process_imu();
    if (!m_odometry_estimator.is_gravity_calibration_underway())
        try_receive_and_process_lidar_scan();
}

void NewCeleris::try_receive_and_process_imu() {
    LOG_METHOD();

    ImuMeasurement imu_message{};
    if (m_imu_receiver.try_pop_back_imu_message(imu_message)) {
        m_odometry_estimator.submit_imu(imu_message);

        // Odometry latest_odometry = m_odometry_estimator.get_latest_odometry();

        // m_lidar_transform.position = latest_odometry.position;
        // m_lidar_transform.rotation = latest_odometry.orientation;
    }
}

void NewCeleris::try_receive_and_process_lidar_scan() {
    LOG_METHOD();

    logger().check(m_engine, "Engine was null");
    logger().check(m_voxel_grid, "Voxel grid was null");

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

    if (m_voxel_point_map.map_point_count() > 0u) {
        m_gicp_pass.fit(m_voxel_point_map,
                        m_network_scan->point_cloud(),
                        m_network_scan->normal_buffer(),
                        m_desc.max_gicp_iterations);
    }

    Odometry last_lidar_odometry{};
    if (m_odometry_estimator.get_last_lidar_odometry(last_lidar_odometry))
        last_lidar_odometry.timestamp_ns = m_network_scan->timestamp();

    m_odometry_estimator.submit_lidar_imu_fusion(
        *m_network_scan, 
        closest_prev_odometry, 
        last_lidar_odometry
    );

    // if (m_lidar_odometry_recorder.is_recording())
    //     m_lidar_odometry_recorder.record(*m_network_scan, m_odometry_estimator.get_latest_odometry());
    
    // if (m_lidar_msg_odom_recorder.is_recording())
    //     m_lidar_msg_odom_recorder.record(lidar_message, m_odometry_estimator.get_latest_odometry());
    
    m_voxel_map_inserter.insert(
        m_voxel_point_map, 
        m_network_scan->point_cloud(),
        m_network_scan->normal_buffer()
    );
    m_voxel_grid->voxelize_point_cloud(
        *m_engine,
        m_network_scan->point_cloud(),
        m_network_scan->normal_buffer(),
        m_voxel_write_list,
        m_desc.max_write_count
    );

    m_received_scan_count++;
}