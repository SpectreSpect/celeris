#include "new_celeris.h"

#include "../vulkan_self/vulkan_submit_context.h"
#include "../voxel_grid_vulkan/voxel_grid.h"
#include "../vulkan_self/vulkan_engine.h"
#include "../managers/manager_bundle.h"
#include "sensors/lidar/lidar_scan.h"
#include "../renderer/transform.h"

NewCeleris::NewCeleris(
    VulkanEngine& engine,
    ManagerBundle& manager_bundle, 
    VulkanQueue& compute_queue,
    VulkanSubmitContext& submit_context,
    VoxelGrid& voxel_grid,
    const VehicleGeometry& vehicle_geometry,
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
            sizeof(uint32_t) * 4 + sizeof(VoxelWriteGPU) * desc.max_write_count)),
        m_path_intersection_detector(
            engine.physical_device(),
            engine.device(),
            submit_context,
            manager_bundle.compute_pass_manager(),
            voxel_grid,
            desc.path_intersection_detector_max_path_points
        ),
        m_path_planner(
            engine,
            submit_context,
            manager_bundle,
            voxel_grid,
            m_path_intersection_detector,
            vehicle_geometry,
            desc.path_planner_desc
        ) {
    LOG_METHOD();
    logger().check(desc.voxel_point_map_num_hash_table_slots > 0, 
                 "The number of voxel point map hash table slots must be greater than 0");
    logger().check(desc.voxel_point_map_max_map_point_count > 0, 
                 "The maximum number of voxel point map points must be greater than 0");
    
    m_voxel_map_reseter.reset(m_voxel_point_map);
}

void NewCeleris::start(VulkanSubmitContext&& planner_submit_context) {
    LOG_METHOD();

    // m_lidar_scan_receiver.start();
    // m_imu_receiver.start();
    m_path_planner.start(std::move(planner_submit_context));
}

void NewCeleris::update() {
    LOG_METHOD();
    
    try_receive_and_process_imu();
    if (!m_odometry_estimator.is_gravity_calibration_underway())
        try_receive_and_process_lidar_scan();
    
    if (m_path_planner_snapshot.generation != m_path_planner.request_result_generation())
        m_path_planner_snapshot = m_path_planner.request_result_snapshot();
}

void NewCeleris::set_start(const NonholonomicPos& position) {
    m_start_position = position;
}

void NewCeleris::set_start(const Camera& camera) {

}

void NewCeleris::set_goal(const NonholonomicPos& position) {
    m_goal_position = position;
}

bool NewCeleris::adjust_to_ground(
    glm::vec3& output,
    int max_step_up,
    int max_drop,
    int max_y_diff,
    bool allow_flying_over_precepices) {
    return m_path_planner.request_adjust_to_ground(
        output,
        max_step_up,
        max_drop,
        max_y_diff,
        allow_flying_over_precepices
    );
}

// void set_goal(const NonholonomicPos& position);

void NewCeleris::request_path_replan() {
    LOG_METHOD();

    m_path_planner.request_path_replan(m_start_position, m_goal_position);
}

OdometryEstimator& NewCeleris::odometry_estimator() {
    return m_odometry_estimator;
}

Transform* NewCeleris::lidar_tranform() {
    if (!m_network_scan)
        return nullptr;
    return &m_network_scan->point_cloud().transform;
}

VoxelGrid* NewCeleris::voxel_grid() {
    return m_voxel_grid;
}

glm::vec3 NewCeleris::voxel_center_bottom_world_pos(const glm::ivec3& voxel_pos) {
    logger().check(m_voxel_grid, "Voxel grid was null");
    
    // return m_path_planner.request_voxel_center_world_pos(voxel_pos);
    return (glm::vec3(voxel_pos) + glm::vec3(0.5f, 0.0f, 0.5f)) * m_voxel_grid->voxel_size();
}

glm::vec3 NewCeleris::voxel_center_world_pos(const glm::ivec3& voxel_pos) {
    logger().check(m_voxel_grid, "Voxel grid was null");
    
    // return m_path_planner.request_voxel_center_world_pos(voxel_pos);
    return (glm::vec3(voxel_pos) + glm::vec3(0.5f)) * m_voxel_grid->voxel_size();
}

NonholonomicPos NewCeleris::start_position() const noexcept {
    return m_start_position;
}

NonholonomicPos NewCeleris::goal_position() const noexcept {
    return m_goal_position;
}

const PathPlanner::PathPlannerResult& NewCeleris::path_planner_snapshot() const noexcept {
    return m_path_planner_snapshot;
}

// NonholonomicPos goal_position();

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