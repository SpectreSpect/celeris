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
        
        m_start_position.from_transform(m_network_scan->point_cloud().transform);        

        // m_planner.initialize(m_start_position, m_goal_position);
        // m_planner.find_nonholomic_path(); // state_explored_paths
        
        // m_voxel_map_inserter.insert(m_voxel_point_map, m_network_scan->point_cloud(), m_network_scan->normal_buffer());
        // m_voxel_grid->voxelize_point_cloud(*m_engine, 
        //                                    m_network_scan->point_cloud(), 
        //                                    voxel_write_list, 
        //                                    m_desc.max_write_count);

        m_received_scan_count++;
    }
}

void Celeris::find_path() {
    m_planner.initialize(m_start_position, m_goal_position);
    m_planner.find_nonholomic_path(); // state_explored_paths
}

void Celeris::set_start(const NonholonomicPos& position) {
    m_start_position = position;
}

void Celeris::set_goal(const NonholonomicPos& position) {
    m_goal_position = position;
}

LidarScan* Celeris::network_scan() {
    return m_network_scan.get();
}

NonholonomicPos Celeris::start_position() const noexcept {
    return m_start_position;
}

NonholonomicPos Celeris::goal_position() const noexcept {
    return m_goal_position;
}

VulkanEngine* Celeris::engine() {
    return m_engine;
}

NonholonomicAStar& Celeris::planner() {
    return m_planner;
}

uint32_t Celeris::received_scan_count() const noexcept {
    return m_received_scan_count;
}