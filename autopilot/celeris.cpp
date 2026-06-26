#include "celeris.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <utility>

#include "../vulkan_self/vulkan_device.h"
#include "../vulkan_self/vulkan_queue.h"
#include "../vulkan_self/vulkan_submit_context.h"
#include "../managers/compute_pass_manager.h"
#include "../managers/manager_bundle.h"

namespace {
    constexpr int COLLISION_BINARY_SEARCH_ITERATIONS = 16;

    float min_component(glm::vec3 value) noexcept {
        return std::min({value.x, value.y, value.z});
    }
}

Celeris::Celeris(VulkanEngine& engine,
                 VulkanQueue& compute_queue,
                 VulkanSubmitContext& submit_context,
                 ManagerBundle& manager_bundle,
                 VoxelGrid& voxel_grid,
                 const CelerisDesc& desc)
    :   m_engine(&engine),
        m_manager_bundle(&manager_bundle),
        m_voxel_grid(&voxel_grid),
        m_desc(desc),
        m_gicp_pass(engine, manager_bundle.compute_pass_manager()),
        m_point_cloud_preprocessor(engine.device(), compute_queue, manager_bundle.compute_pass_manager()),
        m_scan_receiver(m_point_cloud_preprocessor),
        m_unimpended_path_finder(
            engine.physical_device(),
            engine.device(),
            submit_context,
            manager_bundle.compute_pass_manager(),
            voxel_grid,
            desc.unimpended_path_window_size,
            desc.unimpended_path_max_astar_points
        ),
        m_command_sender(),
        m_occupancy_grid(engine.physical_device(), engine.device(), voxel_grid, manager_bundle.compute_pass_manager()),
        m_planner(m_occupancy_grid, desc.nonholonomic_astar_desc, m_unimpended_path_finder),
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
    logger().check(desc.unimpended_path_window_size > 1, "Unimpended path window size must be greater than 1");
    logger().check(desc.unimpended_path_max_astar_points > 0, "Unimpended path max astar points must be greater than 0");
    logger().check(desc.max_write_count > 0, "Max write count must be greater than 0");

    m_voxel_map_reseter.reset(m_voxel_point_map);
}

void Celeris::start_lidar_receiver() {
    LOG_METHOD();

    m_scan_receiver.start();
    m_received_scan_count = 0;
    m_has_previous_lidar_pose = false;
    m_collision_position_history.clear();
}

void Celeris::start(VulkanSubmitContext&& planner_submit_context) {
    start_lidar_receiver();
    // m_command_sender.start();
    start_planner_thread(std::move(planner_submit_context));
}

void Celeris::update() {
    LOG_METHOD();

    logger().check(m_engine, "Engine was null");
    logger().check(m_manager_bundle, "Manager bundle was null");
    logger().check(m_voxel_grid, "Voxel grid was null");

    m_command_sender.set_command(get_path_following_command());

    if (auto scan = m_scan_receiver.try_pop_scan(*m_manager_bundle)) {
        glm::vec3 raw_position = scan->point_cloud().transform.position;
        glm::quat raw_rotation = glm::normalize(scan->point_cloud().transform.rotation);

        if (m_network_scan)
            m_retired_network_scans.push_back(std::move(m_network_scan));

        m_network_scan = std::move(scan);
        std::cout << "Received scan #" << m_received_scan_count << std::endl;

        while (m_retired_network_scans.size() > m_engine->num_frames_in_flight())
            m_retired_network_scans.pop_front();

        if (!m_has_previous_lidar_pose) {
            m_network_scan->point_cloud().transform.position = glm::vec3(0.0f);
            m_network_scan->point_cloud().transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            m_has_previous_lidar_pose = true;
        } else {
            if (glm::dot(m_previous_lidar_rotation, raw_rotation) < 0.0f) {
                raw_rotation = -raw_rotation;
            }

            glm::vec3 delta_position = raw_position - m_previous_lidar_position;
            glm::quat delta_rotation = glm::normalize(raw_rotation * glm::inverse(m_previous_lidar_rotation));

            glm::vec3 previous_map_position = glm::vec3(0.0f);
            glm::quat previous_map_rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

            if (!m_retired_network_scans.empty()) {
                PointCloud& previous_point_cloud = m_retired_network_scans.back()->point_cloud();
                previous_map_position = previous_point_cloud.transform.position;
                previous_map_rotation = glm::normalize(previous_point_cloud.transform.rotation);
            }

            m_network_scan->point_cloud().transform.position = previous_map_position + delta_position;
            m_network_scan->point_cloud().transform.rotation = glm::normalize(delta_rotation * previous_map_rotation);

            m_gicp_pass.fit(m_voxel_point_map,
                            m_network_scan->point_cloud(),
                            m_network_scan->normal_buffer(),
                            m_desc.max_gicp_iterations);
        }
        
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
        m_voxel_grid->voxelize_point_cloud(
            *m_engine, 
            m_network_scan->point_cloud(), 
            voxel_write_list, 
            m_desc.max_write_count
        );

        m_occupancy_grid.adjust_to_ground(
            m_start_position.pos, 
            0, 
            6, 
            6, 
            false
        );

        // Здесь нужно делать коллизию...
        collision(m_collision_position_history, m_start_position.pos);
        remember_collision_position(m_start_position.pos);

        request_path_replan(m_start_position, m_goal_position);

        m_previous_lidar_position = raw_position;
        m_previous_lidar_rotation = raw_rotation;
        m_received_scan_count++;
    }
}

void Celeris::find_path(VulkanSubmitContext& submit_context) {
    total_path_finding_time = AvgTimer();

    total_path_finding_time.start();
    m_planner.initialize(submit_context, m_start_position, m_goal_position);
    m_planner.find_nonholomic_path(); // state_explored_paths
    total_path_finding_time.end();

    std::cout << "Total path finding time: " << total_path_finding_time.average_ms() << " ms" << std::endl;

    {
        std::lock_guard<std::mutex> lock(m_planner_mutex);    
        plain_astar_path = m_planner.state().plain_astar_path;
        nonholonomic_astar_path = m_planner.state().path;
        explored_paths = m_planner.state().explored_paths;
        unimpended_path = m_planner.state().unimpended_astar_positions;
        current_target_path_point_id = 0;
    }
}

void Celeris::set_start(const NonholonomicPos& position) {
    m_start_position = position;
    m_collision_position_history.clear();
}

void Celeris::set_goal(const NonholonomicPos& position) {
    m_goal_position = position;
}

void Celeris::set_car_speed(float speed) noexcept {
    if (std::isfinite(speed))
        m_car_speed = speed;
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

float Celeris::car_speed() const noexcept {
    return m_car_speed;
}

VulkanEngine* Celeris::engine() {
    return m_engine;
}

GICPPass& Celeris::gicp_pass() {
    return m_gicp_pass;
}

VoxelPointMap& Celeris::voxel_point_map() {
    return m_voxel_point_map;
}

VoxelMapPointInserter& Celeris::voxel_map_point_inserter() {
    return m_voxel_map_inserter;
}

VoxelMapPointReseter& Celeris::voxel_map_reseter() {
    return m_voxel_map_reseter;
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

void Celeris::collision(
    std::span<const glm::vec3> previous_points,
    glm::vec3& point_pos)
{
    LOG_METHOD();

    logger().check(m_voxel_grid, "Voxel grid was null");

    auto point_is_free = [&](glm::vec3 point) {
        return !m_occupancy_grid.is_solid(
            m_occupancy_grid.world_to_voxel_pos(point)
        );
    };

    if (point_is_free(point_pos)) {
        return;
    }

    if (previous_points.empty()) {
        return;
    }

    logger().check(
        glm::all(glm::greaterThan(m_occupancy_grid.voxel_size(), glm::vec3(0.0f))),
        "Voxel size must be greater than zero"
    );

    const float min_voxel_size = min_component(m_occupancy_grid.voxel_size());
    const float sample_step = std::max(min_voxel_size * 0.25f, 1e-4f);

    auto find_first_free_on_segment =
        [&](glm::vec3 from, glm::vec3 to, glm::vec3& free_point)
    {
        const glm::vec3 segment = to - from;
        const float segment_length = glm::length(segment);

        if (!std::isfinite(segment_length) || segment_length <= 1e-6f) {
            if (point_is_free(to)) {
                free_point = to;
                return true;
            }

            return false;
        }

        const int sample_count = std::max(
            1,
            static_cast<int>(std::ceil(segment_length / sample_step))
        );

        glm::vec3 last_blocked = from;

        for (int sample_id = 1; sample_id <= sample_count; ++sample_id) {
            const float t = static_cast<float>(sample_id) /
                            static_cast<float>(sample_count);
            const glm::vec3 candidate = glm::mix(from, to, t);

            if (!point_is_free(candidate)) {
                last_blocked = candidate;
                continue;
            }

            glm::vec3 blocked = last_blocked;
            glm::vec3 free = candidate;

            for (int i = 0; i < COLLISION_BINARY_SEARCH_ITERATIONS; ++i) {
                glm::vec3 middle = (blocked + free) * 0.5f;

                if (point_is_free(middle)) {
                    free = middle;
                } else {
                    blocked = middle;
                }
            }

            free_point = free;
            return true;
        }

        return false;
    };

    glm::vec3 segment_start = point_pos;
    glm::vec3 resolved_pos;

    for (glm::vec3 previous_point : previous_points) {
        if (find_first_free_on_segment(segment_start, previous_point, resolved_pos)) {
            point_pos = resolved_pos;
            return;
        }

        segment_start = previous_point;
    }

    std::cerr << "Failed to resolve Celeris collision along previous path. "
              << "Leaving point unchanged.\n";
}

void Celeris::remember_collision_position(glm::vec3 point_pos) {
    if (m_desc.collision_history_size == 0u) {
        m_collision_position_history.clear();
        return;
    }

    if (m_occupancy_grid.is_solid(m_occupancy_grid.world_to_voxel_pos(point_pos))) {
        return;
    }

    m_collision_position_history.insert(
        m_collision_position_history.begin(),
        point_pos
    );

    if (m_collision_position_history.size() > m_desc.collision_history_size) {
        m_collision_position_history.resize(m_desc.collision_history_size);
    }
}

void Celeris::start_planner_thread(VulkanSubmitContext&& submit_context) {
    m_planner_running.exchange(true);
    m_planner_thread = std::thread(&Celeris::planner_loop, this, std::move(submit_context));
}

void Celeris::request_path_replan(const NonholonomicPos& start_pos, const NonholonomicPos& end_pos) {
    m_replan_requested.exchange(true);
}

void Celeris::planner_loop(VulkanSubmitContext submit_context) {
    while (m_planner_running.load()) {
        if (!m_replan_requested.load())
            continue;
        m_replan_requested.exchange(false);

        find_path(submit_context);
    }
}

bool Celeris::find_closest_next_path_point(uint32_t current_id, uint32_t& output_id, uint32_t& output_dist) {
    if (current_id + 1 >= nonholonomic_astar_path.size())
        return false;
    
    if (glm::distance(m_start_position.pos, nonholonomic_astar_path[output_id].pos))


    output_id = current_id + 1;
    output_dist = glm::distance(m_start_position.pos, nonholonomic_astar_path[output_id].pos);
    for (int i = current_id; i < nonholonomic_astar_path.size(); i++) {
        float dist = glm::distance(m_start_position.pos, nonholonomic_astar_path[output_id].pos);
    }
};

VehicleCommand Celeris::get_path_following_command() {
    std::lock_guard<std::mutex> lock(m_planner_mutex);

    if (nonholonomic_astar_path.empty() || current_target_path_point_id >= nonholonomic_astar_path.size())
        return VehicleCommand{
            .speed = 0,
            .steering_angle = 0
        };

    float reach_radius = 1;

    float dist_to_target_point = glm::distance(
        m_start_position.pos, 
        nonholonomic_astar_path[current_target_path_point_id].pos
    );

    if (dist_to_target_point <= reach_radius) {
        
        
        if (nonholonomic_astar_path[current_target_path_point_id].dir == -1) {
            if (!is_stop_waiting) {
                is_stop_waiting = true;
                stop_waiting_start_timestamp = std::chrono::steady_clock::now();
            } else {
                auto current_timestamp = std::chrono::steady_clock::now();
                auto elapsed_time = current_timestamp - stop_waiting_start_timestamp;
                
                double elapsed_seconds = std::chrono::duration<double>(elapsed_time).count();

                if (elapsed_seconds >= stop_waiting_time) {
                    is_stop_waiting = false;
                    current_target_path_point_id++;
                } else {
                    return VehicleCommand{
                        .speed = 0,
                        .steering_angle = 0
                    };
                }
            }
        } 
        else {
            current_target_path_point_id++;
        }    
    }

    float target_theta = nonholonomic_astar_path[current_target_path_point_id].theta;
    float theta_error = NonholonomicAStar::angle_diff(m_start_position.theta, target_theta);

    float direction = nonholonomic_astar_path[current_target_path_point_id].dir;
    float steering = direction * theta_error;

    steering = std::clamp(steering, -0.4f, 0.4f);

    return VehicleCommand{
                .speed = m_car_speed * direction,
                .steering_angle = steering
            };
}
