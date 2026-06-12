#include "celeris.h"

#include "../vulkan_self/vulkan_device.h"
#include "../vulkan_self/vulkan_queue.h"
#include "../managers/compute_pass_manager.h"
#include "../managers/manager_bundle.h"

Celeris::Celeris(VulkanEngine& engine,
                 VulkanQueue& compute_queue, 
                 ManagerBundle& manager_bundle,
                 VoxelGrid& voxel_grid,
                 CelerisDesc desc)
    :   m_engine(&engine),
        m_manager_bundle(&manager_bundle),
        m_voxel_grid(&voxel_grid),
        m_desc(desc),
        m_gicp_pass(engine, manager_bundle.compute_pass_manager()),
        m_point_cloud_preprocessor(engine.device(), compute_queue, manager_bundle.compute_pass_manager()),
        m_scan_receiver(m_point_cloud_preprocessor),
        m_planner(voxel_grid),
        m_voxel_point_map(engine, 
                          desc.voxel_point_map_num_hash_table_slots, 
                          desc.voxel_point_map_max_map_point_count),
        m_voxel_map_inserter(engine, manager_bundle.compute_pass_manager()),
        m_voxel_map_reseter(engine, manager_bundle.compute_pass_manager()),
        voxel_write_list(VulkanBuffer::create_host_visible_storage_buffer(engine, 
                        sizeof(uint32_t) * 4 + sizeof(VoxelWriteGPU) * desc.max_write_count)) {
    LOG_METHOD();

    
    logger.check(desc.voxel_point_map_num_hash_table_slots > 0, 
                 "The number of voxel point map hash table slots must be greater than 0");
    logger.check(desc.voxel_point_map_max_map_point_count > 0, 
                 "The maximum number of voxel point map points must be greater than 0");                 
    logger.check(desc.max_write_count > 0, "Max write count must be greater than 0");

    m_voxel_map_reseter.reset(m_voxel_point_map);
}

void Celeris::start_lidar_receiver() {
    LOG_METHOD();

    m_scan_receiver.start();
    m_received_scan_count = 0;
}

void Celeris::update() {
    LOG_METHOD();

    logger.check(m_engine, "Engine was null");
    logger.check(m_manager_bundle, "Manager bundle was null");
    logger.check(m_voxel_grid, "Voxel grid was null");

    if (auto scan = m_scan_receiver.try_pop_scan(*m_manager_bundle)) {
        if (m_network_scan)
            m_retired_network_scans.push_back(std::move(m_network_scan));

        m_network_scan = std::move(scan);
        std::cout << "Received scan #" << m_received_scan_count << std::endl;

        while (m_retired_network_scans.size() > m_engine->num_frames_in_flight())
            m_retired_network_scans.pop_front();

        if (m_received_scan_count > 0)
            m_gicp_pass.fit(m_voxel_point_map, 
                            m_network_scan->point_cloud(), 
                            m_network_scan->normal_buffer(), 
                            m_desc.max_gicp_iterations);
        
        m_start_position.pos = m_network_scan->point_cloud().transform.position;
        glm::quat q = glm::normalize(m_network_scan->point_cloud().transform.rotation);
        glm::vec3 forward = q * glm::vec3(-1.0f, 0.0f, 0.0f);
        m_start_position.theta = std::atan2(forward.z, forward.x);
        
        // start_sphere.transform.position = m_network_scan->point_cloud().transform.position;
        // start_direction_sphere.transform.position = start_pos.pos + direction_offset(start_pos.theta) * 0.85f + glm::vec3(0, 0.4f, 0);

        // m_planner.initialize(m_start_position, m_goal_position);
        // m_planner.find_nonholomic_path(); // state_explored_paths

        // lines = make_path_lines(planner.state_path);
        // line_cloud.set_lines(lines);
        // explored_path_line_cloud.set_lines(planner.state_explored_paths);

        // has_planned_path = !planner.state_path.empty();
        // path_planning_status = has_planned_path
        //     ? "Path planning finished."
        //     : "Path planning finished with no path.";
        

        // if (make_pose_from_camera(camera, start_pos.theta, start_pos)) {
        //     has_start_pos = true;
        //     has_planned_path = false;
        //     path_planning_status = has_end_pos ? "Start position placed on ground." : "Start position placed on ground. Place end position.";
        //     sync_path_marker_transforms();
        // } else {
        //     path_planning_status = "Could not place start: no ground found near camera.";
        // }
        

        m_voxel_map_inserter.insert(m_voxel_point_map, m_network_scan->point_cloud(), m_network_scan->normal_buffer());
        m_voxel_grid->voxelize_point_cloud(*m_engine, 
                                           m_network_scan->point_cloud(), 
                                           voxel_write_list, 
                                           m_desc.max_write_count);

        m_received_scan_count++;
    }
}

LidarScan* Celeris::network_scan() {
    return m_network_scan.get();
}

NonholonomicPos Celeris::goal_position() const noexcept {
    return m_goal_position;
}