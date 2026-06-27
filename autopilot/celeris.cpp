#include "celeris.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
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

    float length_sq(glm::vec3 value) noexcept {
        return glm::dot(value, value);
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
        m_command_sender(),
        m_path_planner(
            engine,
            submit_context,
            manager_bundle,
            voxel_grid,
            PathPlanner::PathPlannerDesc{
                .unimpended_path_window_size = desc.unimpended_path_window_size,
                .unimpended_path_max_astar_points = desc.unimpended_path_max_astar_points,
                .nonholonomic_astar_desc = desc.nonholonomic_astar_desc
            }
        ),
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
    m_collision_raw_position_history.clear();
    m_has_collision_surface_point = false;
}

void Celeris::start(VulkanSubmitContext&& planner_submit_context) {
    start_lidar_receiver();
    m_command_sender.start();
    m_path_planner.start(std::move(planner_submit_context));
}

void Celeris::update(VulkanSubmitContext& submit_context) {
    LOG_METHOD();

    logger().check(m_engine, "Engine was null");
    logger().check(m_manager_bundle, "Manager bundle was null");
    logger().check(m_voxel_grid, "Voxel grid was null");

    sync_path_planner_result();
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

        m_voxel_map_inserter.insert(m_voxel_point_map, m_network_scan->point_cloud(), m_network_scan->normal_buffer());
        m_voxel_grid->voxelize_point_cloud(
            *m_engine, 
            m_network_scan->point_cloud(), 
            voxel_write_list, 
            m_desc.max_write_count
        );

        m_path_planner.request_adjust_to_ground(
            m_start_position.pos, 
            0, 
            10, 
            10, 
            false
        );

        const glm::vec3 collision_raw_position = m_start_position.pos;

        // Здесь нужно делать коллизию...
        collision(
            m_collision_raw_position_history,
            m_start_position.pos
        );
        remember_collision_raw_position(collision_raw_position);

        if (is_path_impended(submit_context))
            request_path_replan();

        m_previous_lidar_position = raw_position;
        m_previous_lidar_rotation = raw_rotation;
        m_received_scan_count++;
    }
}

void Celeris::set_start(const NonholonomicPos& position) {
    m_start_position = position;
    m_collision_raw_position_history.clear();
    m_has_collision_surface_point = false;
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

uint32_t Celeris::received_scan_count() const noexcept {
    return m_received_scan_count;
}

bool Celeris::collision_point_is_free(glm::vec3 point) {
    return !m_path_planner.request_is_solid_world(point);
}

glm::vec3 Celeris::collision_point_in_voxel_closest_to(
    glm::ivec3 voxel_pos,
    glm::vec3 reference)
{
    const glm::vec3 size = m_path_planner.request_voxel_size();
    const glm::vec3 voxel_min = m_path_planner.request_voxel_to_world_pos(voxel_pos);
    const glm::vec3 voxel_max = voxel_min + size;
    const glm::vec3 epsilon = size * 1e-3f;

    return glm::clamp(reference, voxel_min + epsilon, voxel_max - epsilon);
}

float Celeris::collision_sample_step() {
    const glm::vec3 voxel_size = m_path_planner.request_voxel_size();
    logger().check(
        glm::all(glm::greaterThan(voxel_size, glm::vec3(0.0f))),
        "Voxel size must be greater than zero"
    );

    return std::max(min_component(voxel_size) * 0.25f, 1e-4f);
}

bool Celeris::find_first_free_collision_point_on_segment(
    glm::vec3 from,
    glm::vec3 to,
    glm::vec3& free_point)
{
    const glm::vec3 segment = to - from;
    const float segment_length = glm::length(segment);

    if (!std::isfinite(segment_length) || segment_length <= 1e-6f) {
        if (collision_point_is_free(to)) {
            free_point = to;
            return true;
        }

        return false;
    }

    const float sample_step = collision_sample_step();
    const int sample_count = std::max(
        1,
        static_cast<int>(std::ceil(segment_length / sample_step))
    );

    glm::vec3 last_blocked = from;

    for (int sample_id = 1; sample_id <= sample_count; ++sample_id) {
        const float t = static_cast<float>(sample_id) /
                        static_cast<float>(sample_count);
        const glm::vec3 candidate = glm::mix(from, to, t);

        if (!collision_point_is_free(candidate)) {
            last_blocked = candidate;
            continue;
        }

        glm::vec3 blocked = last_blocked;
        glm::vec3 free = candidate;

        for (int i = 0; i < COLLISION_BINARY_SEARCH_ITERATIONS; ++i) {
            glm::vec3 middle = (blocked + free) * 0.5f;

            if (collision_point_is_free(middle)) {
                free = middle;
            } else {
                blocked = middle;
            }
        }

        free_point = free;
        return true;
    }

    return false;
}

bool Celeris::find_collision_surface_point(
    std::span<const glm::vec3> previous_free_raw_points,
    glm::vec3 point_pos,
    glm::vec3& surface_point)
{
    for (glm::vec3 previous_point : previous_free_raw_points) {
        if (find_first_free_collision_point_on_segment(point_pos, previous_point, surface_point)) {
            return true;
        }
    }

    return false;
}

bool Celeris::find_collision_escape_point(
    glm::vec3 point_pos,
    glm::vec3 direction,
    glm::vec3& resolved_pos)
{
    if (m_desc.collision_escape_search_radius_voxels == 0u) {
        return false;
    }

    const float direction_len_sq = length_sq(direction);
    if (!std::isfinite(direction_len_sq) || direction_len_sq <= 1e-8f) {
        return false;
    }

    const glm::vec3 direction_norm = direction / std::sqrt(direction_len_sq);
    const glm::ivec3 center_voxel = m_path_planner.request_world_to_voxel_pos(point_pos);
    const int radius = static_cast<int>(m_desc.collision_escape_search_radius_voxels);

    bool found = false;
    float best_distance_sq = std::numeric_limits<float>::infinity();
    float best_forward_projection = -std::numeric_limits<float>::infinity();
    glm::vec3 best_pos(0.0f);

    for (int z = -radius; z <= radius; ++z) {
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                glm::ivec3 candidate_voxel = center_voxel + glm::ivec3(x, y, z);
                if (m_path_planner.request_is_solid(candidate_voxel)) {
                    continue;
                }

                glm::vec3 candidate_pos =
                    collision_point_in_voxel_closest_to(candidate_voxel, point_pos);
                glm::vec3 offset = candidate_pos - point_pos;
                float candidate_distance_sq = length_sq(offset);

                if (candidate_distance_sq <= 1e-8f) {
                    continue;
                }

                float forward_projection = glm::dot(offset, direction_norm);
                if (forward_projection <= 0.0f) {
                    continue;
                }

                if (!found ||
                    candidate_distance_sq < best_distance_sq ||
                    (candidate_distance_sq == best_distance_sq &&
                     forward_projection > best_forward_projection))
                {
                    found = true;
                    best_distance_sq = candidate_distance_sq;
                    best_forward_projection = forward_projection;
                    best_pos = candidate_pos;
                }
            }
        }
    }

    if (!found) {
        return false;
    }

    resolved_pos = best_pos;
    return true;
}

void Celeris::collision(
    std::span<const glm::vec3> previous_free_raw_points,
    glm::vec3& point_pos)
{
    LOG_METHOD();

    logger().check(m_voxel_grid, "Voxel grid was null");

    if (collision_point_is_free(point_pos)) {
        return;
    }

    if (previous_free_raw_points.empty() && !m_has_collision_surface_point) {
        return;
    }

    bool has_fallback_surface_point = false;
    glm::vec3 fallback_surface_point(0.0f);
    glm::vec3 escape_direction(0.0f);
    if (m_has_collision_surface_point) {
        escape_direction = m_collision_surface_point - point_pos;
    } else {
        if (!find_collision_surface_point(previous_free_raw_points, point_pos, m_collision_surface_point)) {
            return;
        }

        m_has_collision_surface_point = true;
        fallback_surface_point = m_collision_surface_point;
        has_fallback_surface_point = true;
        escape_direction = point_pos - m_collision_surface_point;
    }

    glm::vec3 resolved_pos;
    if (find_collision_escape_point(point_pos, escape_direction, resolved_pos)) {
        point_pos = resolved_pos;
        m_collision_surface_point = resolved_pos;
        m_has_collision_surface_point = true;
        return;
    }

    if (find_collision_surface_point(previous_free_raw_points, point_pos, resolved_pos)) {
        point_pos = resolved_pos;
        m_collision_surface_point = resolved_pos;
        m_has_collision_surface_point = true;
        return;
    }

    if (m_has_collision_surface_point && collision_point_is_free(m_collision_surface_point)) {
        point_pos = m_collision_surface_point;
        return;
    }

    if (has_fallback_surface_point && collision_point_is_free(fallback_surface_point)) {
        point_pos = fallback_surface_point;
        m_collision_surface_point = fallback_surface_point;
        m_has_collision_surface_point = true;
        return;
    }

    logger().log_error() << "Failed to resolve Celeris collision along previous path. "
                         << "Leaving point unchanged.\n";
}

void Celeris::remember_collision_raw_position(glm::vec3 point_pos) {
    if (m_desc.collision_history_size == 0u) {
        m_collision_raw_position_history.clear();
        m_has_collision_surface_point = false;
        return;
    }

    if (m_path_planner.request_is_solid_world(point_pos)) {
        return;
    }

    m_has_collision_surface_point = false;

    m_collision_raw_position_history.insert(
        m_collision_raw_position_history.begin(),
        point_pos
    );

    if (m_collision_raw_position_history.size() > m_desc.collision_history_size) {
        m_collision_raw_position_history.resize(m_desc.collision_history_size);
    }
}

void Celeris::request_path_replan() {
    m_path_planner.request_path_replan(m_start_position, m_goal_position);
}

bool Celeris::adjust_to_ground(glm::vec3& output) {
    return m_path_planner.request_adjust_to_ground(output);
}

bool Celeris::has_planned_path() const {
    return m_path_planner.request_has_path();
}

PathPlanner::PathPlannerResult Celeris::path_result_snapshot() const {
    return m_path_planner.request_result_snapshot();
}

glm::vec3 Celeris::voxel_size() {
    return m_path_planner.request_voxel_size();
}

glm::vec3 Celeris::voxel_center_world_pos(const glm::ivec3& voxel_pos) {
    return m_path_planner.request_voxel_center_world_pos(voxel_pos);
}

bool Celeris::is_path_impended(VulkanSubmitContext& submit_context) {
    return m_path_planner.request_is_path_impended(submit_context);
}

void Celeris::sync_path_planner_result() {
    PathPlanner::PathPlannerResult result = m_path_planner.request_result_snapshot();
    if (result.generation == m_synced_path_generation)
        return;

    std::lock_guard<std::mutex> lock(m_path_mutex);
    plain_astar_path = std::move(result.plain_astar_path);
    nonholonomic_astar_path = std::move(result.nonholonomic_astar_path);
    explored_paths = std::move(result.explored_paths);
    unimpended_path = std::move(result.unimpended_path);
    total_path_finding_time = result.total_path_finding_time;
    m_synced_path_generation = result.generation;
    current_target_path_point_id = 0;
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

    return true;
};

VehicleCommand Celeris::get_path_following_command() {
    std::lock_guard<std::mutex> lock(m_path_mutex);

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
                    if (current_target_path_point_id >= nonholonomic_astar_path.size())
                        return VehicleCommand{
                            .speed = 0,
                            .steering_angle = 0
                        };
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
            if (current_target_path_point_id >= nonholonomic_astar_path.size())
                return VehicleCommand{
                    .speed = 0,
                    .steering_angle = 0
                };
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
