#include "celeris.h"

#include "../vulkan_self/vulkan_device.h"
#include "../vulkan_self/vulkan_queue.h"
#include "../managers/compute_pass_manager.h"
#include "../managers/manager_bundle.h"

Celeris::Celeris(VulkanEngine& engine,
                 VulkanQueue& compute_queue, 
                 ManagerBundle& manager_bundle,
                 MaterialInstanceManager& material_instance_manager,
                 VoxelGrid& voxel_grid,
                 Voxelizator& voxelizator,
                 VulkanBuffer& scan_vertex_buffer,
                 VulkanBuffer& scan_index_buffer,
                 PointCloudMesher& mesher,
                 CelerisDesc desc)
    :   m_engine(&engine),
        m_manager_bundle(&manager_bundle),
        m_material_instance_manager(&material_instance_manager),
        m_voxel_grid(&voxel_grid),
        m_voxelizator(&voxelizator),
        m_scan_vertex_buffer(&scan_vertex_buffer),
        m_scan_index_buffer(&scan_index_buffer),
        m_mesher(&mesher),
        m_desc(desc),
        m_gicp_pass(engine, manager_bundle.compute_pass_manager()),
        m_point_cloud_preprocessor(engine.device(), compute_queue, manager_bundle.compute_pass_manager()),
        m_scan_receiver(m_point_cloud_preprocessor),
        m_occupancy_grid(voxel_grid),
        m_planner(m_occupancy_grid, desc.nonholonomic_astar_desc),
        m_voxel_point_map(engine, 
                          desc.voxel_point_map_num_hash_table_slots, 
                          desc.voxel_point_map_max_map_point_count),
        m_voxel_map_inserter(engine, manager_bundle.compute_pass_manager()),
        m_voxel_map_reseter(engine, manager_bundle.compute_pass_manager()),
        voxel_write_list(VulkanBuffer::create_host_visible_storage_buffer(engine, 
                        sizeof(uint32_t) * 4 + sizeof(VoxelWriteGPU) * desc.max_write_count)) {
    LOG_METHOD();

    
    logger().check(desc.voxel_point_map_num_hash_table_slots > 0, 
                 "The number of voxel point map hash table slots must be greater than 0");
    logger().check(desc.voxel_point_map_max_map_point_count > 0, 
                 "The maximum number of voxel point map points must be greater than 0");                 
    logger().check(desc.max_write_count > 0, "Max write count must be greater than 0");

    m_voxel_map_reseter.reset(m_voxel_point_map);
}

void Celeris::start_lidar_receiver() {
    LOG_METHOD();

    m_scan_receiver.start();
    m_received_scan_count = 0;
}

void Celeris::start() {
    start_lidar_receiver();
    start_planner_thread();
}

void Celeris::update() {
    LOG_METHOD();

    logger().check(m_engine, "Engine was null");
    logger().check(m_manager_bundle, "Manager bundle was null");
    logger().check(m_voxel_grid, "Voxel grid was null");

    if (auto scan = m_scan_receiver.try_pop_scan(*m_manager_bundle)) {
        if (m_network_scan)
            m_retired_network_scans.push_back(std::move(m_network_scan));

        m_network_scan = std::move(scan);
        std::cout << "Received scan #" << m_received_scan_count << std::endl;

        while (m_retired_network_scans.size() > m_engine->num_frames_in_flight())
            m_retired_network_scans.pop_front();

        uint32_t scan_index_count = m_mesher->convert_to_mesh<PBRVertex, PointInstance>(
            m_network_scan->point_cloud(),
            *m_scan_vertex_buffer,
            *m_scan_index_buffer
        );

        MeshView scan_mesh_view(
            m_scan_vertex_buffer->get_view(),
            m_scan_index_buffer->get_view(),
            scan_index_count
        );

        if (m_received_scan_count > 0)
            m_gicp_pass.fit(m_voxel_point_map, 
                            m_network_scan->point_cloud(), 
                            m_network_scan->normal_buffer(), 
                            m_desc.max_gicp_iterations);
        
        m_start_position.from_transform(m_network_scan->point_cloud().transform);        

        // m_start_position.pos = m_network_scan->point_cloud().transform.position;
        // glm::quat q = glm::normalize(m_network_scan->point_cloud().transform.rotation);
        // glm::vec3 forward = q * glm::vec3(-1.0f, 0.0f, 0.0f);
        // m_start_position.theta = std::atan2(forward.z, forward.x);
        
        // start_sphere.transform.position = m_network_scan->point_cloud().transform.position;
        // start_direction_sphere.transform.position = start_pos.pos + direction_offset(start_pos.theta) * 0.85f + glm::vec3(0, 0.4f, 0);

        // if (m_received_scan_count % path_replanning_interval == 0) {
        //     m_planner.initialize(m_start_position, m_goal_position);
        //     m_planner.find_nonholomic_path(); // state_explored_paths    
        // }

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
        // m_voxel_grid->voxelize_point_cloud(*m_engine, 
        //                                    m_network_scan->point_cloud(), 
        //                                    voxel_write_list, 
        //                                    m_desc.max_write_count);

        VoxelWriteGPU blue_voxelize_prefab;
        blue_voxelize_prefab.voxel_data = VoxelDataGPU(1, VOXEL_VISABILITY_FLAG_BIT, glm::ivec3({0, 98, 255}));
        blue_voxelize_prefab.set_flags = OVERWRITE_BIT;

        RenderObject scan_object(scan_mesh_view, m_material_instance_manager->pbr);
        scan_object.set_material_data(PBRMaterialData::create(0.0f, 0.95f, 1.8f, glm::vec4(1.0f), 1.0f));

        glm::mat4 mesh_matrix = glm::scale(glm::mat4(1.0f), glm::vec3(5.0f)) * 
            m_network_scan->point_cloud().transform.get_model_matrix();

        m_voxelizator->voxelize<PBRVertex>(
            blue_voxelize_prefab,
            scan_object.mesh_view(),
            mesh_matrix,
            &m_voxel_grid->local_voxel_write_list()
        );

        request_path_replan(m_start_position, m_goal_position);

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

std::mutex& Celeris::planner_mutex() noexcept {
    return m_planner_mutex;
}

void Celeris::start_planner_thread() {
    m_planner_running.exchange(true);
    m_planner_thread = std::thread(&Celeris::planner_loop, this);
}

void Celeris::request_path_replan(const NonholonomicPos& start_pos, const NonholonomicPos& end_pos) {
    m_replan_requested.exchange(true);
}

void Celeris::planner_loop() {
    while (m_planner_running.load()) {
        if (!m_replan_requested.load())
            continue;
        m_replan_requested.exchange(false);

        m_planner.initialize(m_start_position, m_goal_position);
        m_planner.find_nonholomic_path();

        {
            std::lock_guard<std::mutex> lock(m_planner_mutex);    
            plain_astar_path = m_planner.state().plain_astar_path;
            nonholonomic_astar_path = m_planner.state().path;
        }
    }
}