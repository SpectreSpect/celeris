#include "celeris.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <utility>
#include <glm/gtc/quaternion.hpp>

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

    bool is_finite(float value) noexcept {
        return std::isfinite(value);
    }

    bool is_finite(glm::vec3 value) noexcept {
        return
            is_finite(value.x) &&
            is_finite(value.y) &&
            is_finite(value.z);
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
        m_scan_receiver(m_point_cloud_preprocessor, desc.receiver_port),
        m_unimpended_path_finder(
            engine.physical_device(),
            engine.device(),
            submit_context,
            manager_bundle.compute_pass_manager(),
            voxel_grid,
            desc.unimpended_path_window_size,
            desc.unimpended_path_max_astar_points
        ),
        m_path_intersection_detector(
            engine.physical_device(),
            engine.device(),
            submit_context,
            manager_bundle.compute_pass_manager(),
            voxel_grid,
            desc.unimpended_path_max_astar_points
        ),
        m_command_sender(),
        m_vehicle_state_receiver(desc.vehicle_state_receiver_port),
        m_occupancy_grid(engine.physical_device(), engine.device(), voxel_grid, manager_bundle.compute_pass_manager()),
        m_vehicle(
            desc.max_vehicle_acceleration,
            desc.max_vehicle_steer_acceleration,
            desc.vehicle_wheel_base
        ),
        m_planner(m_occupancy_grid, desc.nonholonomic_astar_desc, m_unimpended_path_finder),
        m_local_planner(),
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
    m_vehicle_state_receiver.start();
    m_command_sender.start();
    start_planner_thread(std::move(planner_submit_context));
}

void Celeris::update(VulkanSubmitContext& submit_context) {
    LOG_METHOD();

    logger().check(m_engine, "Engine was null");
    logger().check(m_manager_bundle, "Manager bundle was null");
    logger().check(m_voxel_grid, "Voxel grid was null");

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

        m_local_planner.update_timestamp();

        VehicleFeedback vehicle_feedback;
        const bool has_vehicle_feedback =
            m_vehicle_state_receiver.latest_feedback(vehicle_feedback) &&
            is_vehicle_feedback_fresh(vehicle_feedback);

        if (!has_vehicle_feedback || !vehicle_feedback.has_vehicle_state()) {
            m_local_planner.predict_vehicle_state(m_vehicle); // Предсказываем состояние машины к текущему моменту
        }
        
        // Актуализируем состояние машины в соответствии с данных с сенсоров
        const Transform& vehicle_transform = m_network_scan->point_cloud().transform;
        m_vehicle.state().m_position = glm::vec2{
            vehicle_transform.position.x,
            vehicle_transform.position.z
        };

        const glm::quat vehicle_rotation = glm::normalize(vehicle_transform.rotation);
        const glm::vec3 vehicle_forward = vehicle_rotation * glm::vec3(-1.0f, 0.0f, 0.0f);
        m_vehicle.state().m_heading = std::atan2(vehicle_forward.z, vehicle_forward.x);

        if (has_vehicle_feedback) {
            apply_vehicle_feedback(vehicle_feedback);
        }

        m_vehicle_position.pos = glm::vec3{
            m_vehicle.state().m_position.x,
            vehicle_transform.position.y,
            m_vehicle.state().m_position.y
        };
        m_vehicle_position.theta = m_vehicle.state().m_heading;
        m_vehicle_position.steer = m_vehicle.state().m_steering_angle;
        if (std::abs(m_vehicle.state().m_speed) > 1e-4f) {
            m_vehicle_position.dir = m_vehicle.state().m_speed < 0.0f ? -1.0f : 1.0f;
        }
        
        /*
            Отправляем следующую команду управления (состояние машины в соответствии с этой 
            командой не обнавляется. Оно обновится когда прийдут следующие данные сенсоров)
        */
        m_command_sender.set_command(get_path_following_command(submit_context));

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

        const glm::vec3 collision_raw_position = m_start_position.pos;

        // Здесь нужно делать коллизию...
        collision(
            m_collision_raw_position_history,
            m_start_position.pos
        );
        remember_collision_raw_position(collision_raw_position);

        request_path_replan(m_start_position, m_goal_position);

        m_previous_lidar_position = raw_position;
        m_previous_lidar_rotation = raw_rotation;
        m_received_scan_count++;
    }
}

void Celeris::apply_vehicle_feedback(const VehicleFeedback& feedback) {
    Vehicle::VehicleTransformState& state = m_vehicle.state();

    if (feedback.has_odometry()) {
        if (!feedback.has_vehicle_state() && is_finite(feedback.linear_velocity_ros)) {
            const glm::vec3 linear_velocity_engine =
                LidarScan::ros_pos_to_engine(feedback.linear_velocity_ros);
            const glm::vec2 forward{
                std::cos(state.m_heading),
                std::sin(state.m_heading)
            };
            state.m_speed = glm::dot(
                glm::vec2{linear_velocity_engine.x, linear_velocity_engine.z},
                forward
            );
        }
    }

    if (feedback.has_vehicle_state()) {
        if (std::isfinite(feedback.speed))
            state.m_speed = feedback.speed;
        if (std::isfinite(feedback.acceleration))
            state.m_speed_acceleration = feedback.acceleration;
        if (std::isfinite(feedback.steering_angle))
            state.m_steering_angle = feedback.steering_angle;
        if (std::isfinite(feedback.steering_angle_velocity))
            state.m_steering_angle_velocity = feedback.steering_angle_velocity;
        if (std::isfinite(feedback.steering_angle_acceleration))
            state.m_steering_angle_acceleration = feedback.steering_angle_acceleration;
    }
}

bool Celeris::is_vehicle_feedback_fresh(const VehicleFeedback& feedback) const {
    const std::chrono::duration<float> age =
        std::chrono::steady_clock::now() - feedback.received_at;
    return age.count() <= m_desc.vehicle_state_timeout;
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
    m_vehicle_position = position;
    m_vehicle.state().m_position = glm::vec2{position.pos.x, position.pos.z};
    m_vehicle.state().m_heading = position.theta;
    m_vehicle.state().m_steering_angle = position.steer;
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

NonholonomicPos Celeris::vehicle_position() const noexcept {
    return m_vehicle_position;
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

const std::vector<Vehicle::SimulationControlCandidate>&
Celeris::local_planner_candidates() const noexcept
{
    return m_local_planner.last_simulation_candidates();
}

uint32_t Celeris::received_scan_count() const noexcept {
    return m_received_scan_count;
}

std::mutex& Celeris::planner_mutex() noexcept {
    return m_planner_mutex;
}

bool Celeris::collision_point_is_free(glm::vec3 point) {
    return !m_occupancy_grid.is_solid(
        m_occupancy_grid.world_to_voxel_pos(point)
    );
}

glm::vec3 Celeris::collision_point_in_voxel_closest_to(
    glm::ivec3 voxel_pos,
    glm::vec3 reference)
{
    const glm::vec3 voxel_size = m_occupancy_grid.voxel_size();
    const glm::vec3 voxel_min = m_occupancy_grid.voxel_to_world_pos(voxel_pos);
    const glm::vec3 voxel_max = voxel_min + voxel_size;
    const glm::vec3 epsilon = voxel_size * 1e-3f;

    return glm::clamp(reference, voxel_min + epsilon, voxel_max - epsilon);
}

float Celeris::collision_sample_step() {
    const glm::vec3 voxel_size = m_occupancy_grid.voxel_size();
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
    const glm::ivec3 center_voxel = m_occupancy_grid.world_to_voxel_pos(point_pos);
    const int radius = static_cast<int>(m_desc.collision_escape_search_radius_voxels);

    bool found = false;
    float best_distance_sq = std::numeric_limits<float>::infinity();
    float best_forward_projection = -std::numeric_limits<float>::infinity();
    glm::vec3 best_pos(0.0f);

    for (int z = -radius; z <= radius; ++z) {
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                glm::ivec3 candidate_voxel = center_voxel + glm::ivec3(x, y, z);
                if (m_occupancy_grid.is_solid(candidate_voxel)) {
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

    if (m_occupancy_grid.is_solid(m_occupancy_grid.world_to_voxel_pos(point_pos))) {
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

        std::vector<glm::vec3> noncholonomic_path_points;
        noncholonomic_path_points.reserve(m_planner.state().path.size());

        for (const NonholonomicPos& point : m_planner.state().path) {
            noncholonomic_path_points.push_back(point.pos);
        }
        
        if (noncholonomic_path_points.size() == 0 || 
            m_path_intersection_detector.has_intersection(submit_context, noncholonomic_path_points)) {
            find_path(submit_context);
        }
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

VehicleCommand Celeris::get_path_following_command(VulkanSubmitContext& submit_context) {
    std::lock_guard<std::mutex> lock(m_planner_mutex);

    // if (nonholonomic_astar_path.empty() || current_target_path_point_id >= nonholonomic_astar_path.size())
    //     return VehicleCommand{
    //         .speed = 0,
    //         .steering_angle = 0
    //     };

    // float reach_radius = 1;

    // float dist_to_target_point = glm::distance(
    //     m_start_position.pos, 
    //     nonholonomic_astar_path[current_target_path_point_id].pos
    // );

    // if (dist_to_target_point <= reach_radius) {
        
        
    //     if (nonholonomic_astar_path[current_target_path_point_id].dir == -1) {
    //         if (!is_stop_waiting) {
    //             is_stop_waiting = true;
    //             stop_waiting_start_timestamp = std::chrono::steady_clock::now();
    //         } else {
    //             auto current_timestamp = std::chrono::steady_clock::now();
    //             auto elapsed_time = current_timestamp - stop_waiting_start_timestamp;
                
    //             double elapsed_seconds = std::chrono::duration<double>(elapsed_time).count();

    //             if (elapsed_seconds >= stop_waiting_time) {
    //                 is_stop_waiting = false;
    //                 current_target_path_point_id++;
    //             } else {
    //                 return VehicleCommand{
    //                     .speed = 0,
    //                     .steering_angle = 0
    //                 };
    //             }
    //         }
    //     } 
    //     else {
    //         current_target_path_point_id++;
    //     }    
    // }

    // float target_theta = nonholonomic_astar_path[current_target_path_point_id].theta;
    // float theta_error = NonholonomicAStar::angle_diff(m_start_position.theta, target_theta);

    // float direction = nonholonomic_astar_path[current_target_path_point_id].dir;
    // float steering = direction * theta_error;

    // steering = std::clamp(steering, -0.4f, 0.4f);


    
    m_local_planner.set_astar_path(nonholonomic_astar_path);
    return m_local_planner.predict_vehicle_command(m_vehicle, m_path_intersection_detector, submit_context);
}
