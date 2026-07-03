#include "celeris.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <iostream>
#include <limits>
#include <utility>
#include <glm/gtc/quaternion.hpp>

#include <glm/gtc/constants.hpp>
#include "../vulkan_self/vulkan_device.h"
#include "../vulkan_self/vulkan_queue.h"
#include "../vulkan_self/vulkan_submit_context.h"
#include "../managers/compute_pass_manager.h"
#include "../managers/manager_bundle.h"

namespace {
    constexpr int COLLISION_BINARY_SEARCH_ITERATIONS = 16;
    constexpr float PATH_DIRECTION_CLEANUP_EPS = 1e-5f;

    struct DirectionRun {
        size_t first_segment = 1;
        size_t past_last_segment = 1;
        float dir = 1.0f;
        float length = 0.0f;
    };

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

    float normalized_path_dir(float dir) noexcept {
        return dir < 0.0f ? -1.0f : 1.0f;
    }

    float path_segment_length_xz(const NonholonomicPos& from, const NonholonomicPos& to) noexcept {
        const glm::vec2 delta{
            to.pos.x - from.pos.x,
            to.pos.z - from.pos.z
        };
        return glm::length(delta);
    }

    std::vector<DirectionRun> build_direction_runs(const std::vector<NonholonomicPos>& path) {
        std::vector<DirectionRun> runs;
        if (path.size() < 2)
            return runs;

        for (size_t segment = 1; segment < path.size(); segment++) {
            const float segment_dir = normalized_path_dir(path[segment].dir);
            const float segment_length = path_segment_length_xz(path[segment - 1], path[segment]);

            if (runs.empty() || runs.back().dir != segment_dir) {
                runs.push_back(DirectionRun{
                    .first_segment = segment,
                    .past_last_segment = segment + 1,
                    .dir = segment_dir,
                    .length = segment_length
                });
            } else {
                runs.back().past_last_segment = segment + 1;
                runs.back().length += segment_length;
            }
        }

        return runs;
    }

    uint32_t cleanup_short_direction_runs(
        std::vector<NonholonomicPos>& path,
        float min_segment_length)
    {
        if (path.size() < 4 || min_segment_length <= PATH_DIRECTION_CLEANUP_EPS)
            return 0;

        uint32_t rewritten_segments = 0;
        for (size_t pass = 0; pass < path.size(); pass++) {
            const std::vector<DirectionRun> runs = build_direction_runs(path);
            if (runs.size() < 3)
                break;

            bool changed = false;
            for (size_t run_id = 1; run_id + 1 < runs.size(); run_id++) {
                const DirectionRun& run = runs[run_id];
                if (run.length >= min_segment_length)
                    continue;

                const DirectionRun& left = runs[run_id - 1];
                const DirectionRun& right = runs[run_id + 1];
                const float replacement_dir =
                    left.dir == right.dir
                        ? left.dir
                        : (left.length >= right.length ? left.dir : right.dir);

                if (replacement_dir == run.dir)
                    continue;

                for (size_t segment = run.first_segment;
                     segment < run.past_last_segment && segment < path.size();
                     segment++)
                {
                    if (normalized_path_dir(path[segment].dir) != replacement_dir) {
                        path[segment].dir = replacement_dir;
                        rewritten_segments++;
                    }
                }

                changed = true;
            }

            if (!changed)
                break;
        }

        if (path.size() > 1)
            path.front().dir = path[1].dir;

        return rewritten_segments;
    }

    glm::vec3 vehicle_geometry_position_to_model_offset(
        const VehicleGeometry& vehicle_geometry,
        glm::vec3 position_from_left_rear_bottom
    ) {
        return glm::vec3(
            position_from_left_rear_bottom.z - vehicle_geometry.size.z / 2.0f,
            position_from_left_rear_bottom.y - vehicle_geometry.size.y / 2.0f,
            position_from_left_rear_bottom.x - vehicle_geometry.size.x / 2.0f
        );
    }
}

Celeris::Celeris(VulkanEngine& engine,
                 VulkanQueue& compute_queue,
                 VulkanSubmitContext& submit_context,
                 ManagerBundle& manager_bundle,
                 MaterialInstanceManager& material_instance_manager,
                 VoxelGrid& voxel_grid,
                 Voxelizator& voxelizator,
                 VulkanBuffer& scan_vertex_buffer,
                 VulkanBuffer& scan_index_buffer,
                 PointCloudMesher& mesher,
                 const CelerisDesc& desc)
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
        m_path_intersection_detector(
            engine.physical_device(),
            engine.device(),
            submit_context,
            manager_bundle.compute_pass_manager(),
            voxel_grid,
            1024
        ),
        m_point_cloud_preprocessor(engine.device(), compute_queue, manager_bundle.compute_pass_manager()),
        m_scan_receiver(m_point_cloud_preprocessor, desc.receiver_port),
        m_command_sender(),
        m_vehicle_state_receiver(desc.vehicle_state_receiver_port),
        m_vehicle(
            desc.max_vehicle_acceleration,
            desc.max_vehicle_steer_acceleration,
            desc.vehicle_wheel_base
        ),
        m_path_planner(
            engine,
            submit_context,
            manager_bundle,
            voxel_grid,
            m_path_intersection_detector,
            PathPlanner::PathPlannerDesc{
                .unimpended_path_window_size = desc.unimpended_path_window_size,
                .unimpended_path_max_astar_points = desc.unimpended_path_max_astar_points,
                .vehicle_geometry = desc.vehicle_geometry,
                .footprint_sample_count = desc.footprint_sample_count,
                .footprint_horizontal_inflation_size = desc.footprint_horizontal_inflation_size,
                .footprint_vertical_inflation_size = desc.footprint_vertical_inflation_size,
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

    Vehicle::SimulationFollowParams& follow_params = m_vehicle.follow_params();
    follow_params.cruise_speed = std::max(0.0f, desc.vehicle_cruise_speed);
    follow_params.slowdown_distance_from_path = std::max(0.0f, desc.vehicle_slowdown_distance_from_path);
    follow_params.min_off_path_speed_factor = std::clamp(
        desc.vehicle_min_off_path_speed_factor,
        0.0f,
        1.0f
    );
    follow_params.projection_backtrack_window = std::max(0.0f, desc.vehicle_projection_backtrack_window);
    follow_params.projection_lookahead_base = std::max(0.0f, desc.vehicle_projection_lookahead_base);
    follow_params.min_direction_segment_virtual_length =
        std::max(0.0f, desc.vehicle_min_direction_segment_virtual_length);
    follow_params.direction_switch_arrival_distance =
        std::max(0.0f, desc.vehicle_direction_switch_arrival_distance);
    follow_params.direction_switch_arrival_speed =
        std::max(0.0f, desc.vehicle_direction_switch_arrival_speed);
    follow_params.direction_switch_approach_speed =
        std::max(0.0f, desc.vehicle_direction_switch_approach_speed);
    m_car_speed = follow_params.cruise_speed;

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
    m_path_planner.start(std::move(planner_submit_context));
}

void Celeris::update(VulkanSubmitContext& submit_context) {
    logger().check(m_engine, "Engine was null");
    logger().check(m_manager_bundle, "Manager bundle was null");
    logger().check(m_voxel_grid, "Voxel grid was null");

    sync_path_planner_result();

    m_local_planner.update_timestamp();

    VehicleFeedback vehicle_feedback;
    const bool has_vehicle_feedback =
        m_vehicle_state_receiver.latest_feedback(vehicle_feedback) &&
        is_vehicle_feedback_fresh(vehicle_feedback);
    if (has_vehicle_feedback) {
        apply_vehicle_feedback(vehicle_feedback);
    }
    m_local_planner.predict_vehicle_state(m_vehicle);

    float vehicle_height = m_vehicle_position.pos.y;

    if (auto scan = m_scan_receiver.try_pop_scan(*m_manager_bundle)) {
        if (m_network_scan)
            m_retired_network_scans.push_back(std::move(m_network_scan));

        m_network_scan = std::move(scan);
        // std::cout << "Received scan #" << m_received_scan_count << std::endl;

        while (m_retired_network_scans.size() > m_engine->num_frames_in_flight())
            m_retired_network_scans.pop_front();

        // uint32_t scan_index_count = m_mesher->convert_to_mesh<PBRVertex, PointInstance>(
        //     m_network_scan->point_cloud(),
        //     *m_scan_vertex_buffer,
        //     *m_scan_index_buffer
        // );

        // MeshView scan_mesh_view(
        //     m_scan_vertex_buffer->get_view(),
        //     m_scan_index_buffer->get_view(),
        //     scan_index_count
        // );

        if (!m_has_previous_lidar_pose) {
            m_network_scan->point_cloud().transform.position = glm::vec3(0.0f);
            m_network_scan->point_cloud().transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            m_has_previous_lidar_pose = true;
        } else {
            const Vehicle::VehicleTransformState& state = m_vehicle.state();
            Transform predicted_rear_axle_transform;
            predicted_rear_axle_transform.position = glm::vec3{
                state.m_position.x,
                vehicle_height,
                state.m_position.y
            };
            predicted_rear_axle_transform.rotation = glm::angleAxis(
                glm::pi<float>() - state.m_heading,
                glm::vec3(0.0f, 1.0f, 0.0f)
            );
            m_network_scan->point_cloud().transform =
                lidar_transform_from_rear_axle_transform(predicted_rear_axle_transform);

            m_gicp_pass.fit(m_voxel_point_map,
                            m_network_scan->point_cloud(),
                            m_network_scan->normal_buffer(),
                            m_desc.max_gicp_iterations);
        }
        
        m_lidar_transform = m_network_scan->point_cloud().transform;
        const Transform vehicle_transform = rear_axle_transform_from_lidar_transform(m_lidar_transform);
        NonholonomicPos vehicle_pose;
        vehicle_pose.from_transform(vehicle_transform);
        vehicle_height = vehicle_transform.position.y;

        // Корректируем позу машины по lidar/GICP. Скорость и руль приходят отдельно.
        m_vehicle.state().m_position = glm::vec2{
            vehicle_transform.position.x,
            vehicle_transform.position.z
        };

        const glm::quat vehicle_rotation = glm::normalize(vehicle_transform.rotation);
        const glm::vec3 vehicle_forward = vehicle_rotation * glm::vec3(-1.0f, 0.0f, 0.0f);
        m_vehicle.state().m_heading = std::atan2(vehicle_forward.z, vehicle_forward.x);

        m_voxel_map_inserter.insert(m_voxel_point_map, m_network_scan->point_cloud(), m_network_scan->normal_buffer());
        m_voxel_grid->voxelize_point_cloud(
            *m_engine, 
            m_network_scan->point_cloud(), 
            m_network_scan->normal_buffer(),
            voxel_write_list, 
            m_desc.max_write_count
        );

        m_path_planner.request_adjust_to_ground(
            vehicle_pose.pos,
            0, 
            6, 
            6, 
            false
        );

        const glm::vec3 collision_raw_position = vehicle_pose.pos;

        collision(
            m_collision_raw_position_history,
            vehicle_pose.pos
        );
        remember_collision_raw_position(collision_raw_position);

        if (has_planned_path() && is_path_impended(submit_context))
            request_path_replan();

        m_received_scan_count++;
    }

    sync_vehicle_position_from_state(vehicle_height);

    const auto now = std::chrono::steady_clock::now();
    const float local_planner_update_period = std::max(0.0f, m_desc.local_planner_update_period);
    const bool should_update_local_planner =
        !m_has_last_local_planner_update_timestamp ||
        local_planner_update_period <= 0.0f ||
        std::chrono::duration<float>(now - m_last_local_planner_update_timestamp).count() >=
            local_planner_update_period;

    if (should_update_local_planner) {
        VehicleCommand vehicle_command = m_local_planner.step(
            m_vehicle,
            m_path_intersection_detector,
            submit_context
        );

        m_command_sender.set_command(vehicle_command);
        m_last_local_planner_update_timestamp = now;
        m_has_last_local_planner_update_timestamp = true;
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

void Celeris::sync_vehicle_position_from_state(float height) {
    const Vehicle::VehicleTransformState& state = m_vehicle.state();

    m_vehicle_position.pos = glm::vec3{
        state.m_position.x,
        height,
        state.m_position.y
    };
    m_vehicle_position.theta = state.m_heading;
    m_vehicle_position.steer = state.m_steering_angle;
    if (std::abs(state.m_speed) > 1e-4f) {
        m_vehicle_position.dir = state.m_speed < 0.0f ? -1.0f : 1.0f;
    }
}

void Celeris::set_start(const NonholonomicPos& position) {
    m_start_position = position;
    m_has_start_position = true;
}

void Celeris::set_goal(const NonholonomicPos& position) {
    m_goal_position = position;
    m_has_goal_position = true;
}

void Celeris::set_car_speed(float speed) noexcept {
    if (!std::isfinite(speed))
        return;

    m_car_speed = std::max(0.0f, speed);
    m_vehicle.follow_params().cruise_speed = m_car_speed;
}

LidarScan* Celeris::network_scan() {
    return m_network_scan.get();
}

const Transform& Celeris::lidar_transform() const noexcept {
    return m_lidar_transform;
}

bool Celeris::has_start_position() const noexcept {
    return m_has_start_position;
}

bool Celeris::has_goal_position() const noexcept {
    return m_has_goal_position;
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
    return m_vehicle.follow_params().cruise_speed;
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

const std::vector<Vehicle::SimulationControlCandidate>&
Celeris::local_planner_candidates() const noexcept
{
    return m_local_planner.last_simulation_candidates();
}

float Celeris::local_planner_path_window_min_s() const noexcept {
    return m_local_planner.path_window_min_s();
}

float Celeris::local_planner_path_window_max_s() const noexcept {
    return m_local_planner.path_window_max_s();
}

Footprint& Celeris::footprint() noexcept {
    return m_path_planner.footprint();
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

bool Celeris::request_path_replan() {
    if (!m_has_start_position || !m_has_goal_position)
        return false;

    m_path_planner.request_path_replan(m_start_position, m_goal_position);
    return true;
}

bool Celeris::adjust_to_ground(
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

bool Celeris::adjust_to_ground(
    std::vector<glm::vec3>& output,
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

bool Celeris::get_ground_positions(
    const std::vector<glm::vec3>& polyline,
    std::vector<glm::ivec3>& output,
    int max_step_up,
    int max_drop,
    int max_y_diff,
    bool allow_flying_over_precepices) {
    return m_path_planner.request_get_ground_positions(
        polyline,
        output,
        max_step_up,
        max_drop,
        max_y_diff,
        allow_flying_over_precepices
    );
}

bool Celeris::has_planned_path() const {
    return m_path_planner.request_has_path();
}

PathPlanner::PathPlannerResult Celeris::path_result_snapshot() const {
    PathPlanner::PathPlannerResult result = m_path_planner.request_result_snapshot();

    std::lock_guard<std::mutex> lock(m_path_mutex);
    if (result.generation == m_synced_path_generation &&
        m_path_direction_cleanup_revision == m_synced_path_direction_cleanup_revision)
    {
        result.plain_astar_path = plain_astar_path;
        result.nonholonomic_astar_path = nonholonomic_astar_path;
        result.explored_paths = explored_paths;
        result.unimpended_path = unimpended_path;
        result.total_path_finding_time = total_path_finding_time;
    }

    return result;
}

std::vector<NonholonomicPos> Celeris::get_nonholonomic_astar_path() const {
    LOG_METHOD();

    std::lock_guard<std::mutex> lock(m_path_mutex);
    return nonholonomic_astar_path;
}

void Celeris::display_path_planner_debug_controls() {
    if (!ImGui::CollapsingHeader("Path planner")) {
        return;
    }

    PathPlanner::PathPlannerResult result = path_result_snapshot();

    ImGui::Text("Start marker: %s", m_has_start_position ? "set" : "missing");
    ImGui::Text("Goal marker: %s", m_has_goal_position ? "set" : "missing");
    ImGui::Text(
        "Replan: requests=%llu started=%llu pending=%d planning=%d",
        static_cast<unsigned long long>(m_path_planner.request_replan_request_count()),
        static_cast<unsigned long long>(m_path_planner.request_replan_start_count()),
        m_path_planner.request_has_pending_replan() ? 1 : 0,
        m_path_planner.request_is_planning() ? 1 : 0
    );
    ImGui::Text("Generation: %llu", static_cast<unsigned long long>(result.generation));
    ImGui::Text("Total: %.3f ms", result.total_time_ms);
    ImGui::Separator();
    ImGui::Text("Initialize: %.3f ms", result.initialize_time_ms);
    ImGui::Text("Plain A*: %.3f ms", result.plain_astar_time_ms);
    ImGui::Text("Unimpeded path: %.3f ms", result.unimpended_path_time_ms);
    ImGui::Text("Nonholonomic A*: %.3f ms", result.nonholonomic_astar_time_ms);
    ImGui::Text("is_solid: %.3f ms (%u checks)", result.is_solid_time_ms, result.is_solid_count);
    ImGui::Separator();
    ImGui::Text("Plain A* points: %zu", result.plain_astar_path.path.size());
    ImGui::Text("Unimpeded points: %zu", result.unimpended_path.size());
    ImGui::Text("Nonholonomic points: %zu", result.nonholonomic_astar_path.size());
    ImGui::Text("Explored segments: %zu", result.explored_paths.size());
    if (ImGui::DragFloat(
            "Direction cleanup min segment",
            &m_desc.global_path_direction_cleanup_min_segment_length,
            0.05f,
            0.0f,
            20.0f,
            "%.3f"))
    {
        m_desc.global_path_direction_cleanup_min_segment_length =
            std::max(0.0f, m_desc.global_path_direction_cleanup_min_segment_length);
        m_path_direction_cleanup_revision++;
    }

    ImGui::Separator();
    ImGui::Text("Command sender: %s, %s",
        m_command_sender.is_running() ? "running" : "stopped",
        m_command_sender.is_connected() ? "connected" : "disconnected");
    ImGui::Text(
        "Command packets: %llu sent, %llu failures",
        static_cast<unsigned long long>(m_command_sender.sent_packet_count()),
        static_cast<unsigned long long>(m_command_sender.send_failure_count())
    );

    const VehicleCommand command = m_command_sender.command();
    ImGui::Text("Last command acceleration: %.3f", command.acceleration);
    ImGui::Text("Last command steering velocity: %.3f", command.steering_angle_velocity);

    VehicleFeedback feedback;
    const bool has_feedback = m_vehicle_state_receiver.latest_feedback(feedback);
    const bool fresh_feedback = has_feedback && is_vehicle_feedback_fresh(feedback);
    ImGui::Text("Vehicle feedback: %s, %s",
        has_feedback ? "received" : "none",
        fresh_feedback ? "fresh" : "stale");
    if (has_feedback) {
        ImGui::Text("Feedback flags: odom=%d state=%d",
            feedback.has_odometry() ? 1 : 0,
            feedback.has_vehicle_state() ? 1 : 0);
    }

    const Vehicle::VehicleTransformState& state = m_vehicle.state();
    const Vehicle::SimulationFollowParams& follow_params = m_vehicle.follow_params();
    ImGui::Text("Vehicle speed: %.3f", state.m_speed);
    ImGui::Text("Vehicle cruise speed: %.3f", follow_params.cruise_speed);
    ImGui::Text("Vehicle acceleration: %.3f", state.m_speed_acceleration);
    ImGui::Text("Steering angle: %.3f", state.m_steering_angle);
    ImGui::Text("Steering velocity: %.3f", state.m_steering_angle_velocity);

    const std::vector<Vehicle::SimulationControlCandidate>& candidates =
        m_local_planner.last_simulation_candidates();
    ImGui::Text("Local candidates: %zu", candidates.size());
    if (ImGui::DragFloat("Local planner update period", &m_desc.local_planner_update_period, 0.001f, 0.0f, 0.2f, "%.3f")) {
        m_desc.local_planner_update_period = std::max(0.0f, m_desc.local_planner_update_period);
    }
    ImGui::Text(
        "Local path progress: %.2f / %.2f",
        m_local_planner.path_progress_s(),
        m_local_planner.path_length()
    );
    ImGui::Text(
        "Local s window: %.2f .. %.2f",
        m_local_planner.path_window_min_s(),
        m_local_planner.path_window_max_s()
    );
    ImGui::Text(
        "Local path generation: %llu",
        static_cast<unsigned long long>(m_local_planner.path_generation())
    );
    if (!candidates.empty()) {
        const Vehicle::SimulationControlCandidate& best = candidates.front();
        const Vehicle::SimulationControlCandidateDebug& debug = best.debug;
        ImGui::Text("Best loss: %.3f", best.loss);
        ImGui::Text("Best acceleration: %.3f", best.control_command.speed_acceleration);
        ImGui::Text("Best steering acceleration: %.3f", best.control_command.steer_acceleration);
        ImGui::Text("Best trajectory length: %.3f", debug.trajectory_length);
        ImGui::Text(
            "Best s: %.2f -> %.2f, target %.2f",
            debug.start_s,
            debug.end_s,
            debug.target_end_s
        );
        ImGui::Text(
            "Best dist to path: %.3f -> %.3f",
            debug.start_dist,
            debug.end_dist
        );
        ImGui::Text("Best target end error: %.3f", debug.target_end_dist);
        ImGui::Text("Best reference path speed: %.3f", debug.reference_initial_path_speed);
        ImGui::Text("Best predicted speed: %.3f", best.predicted_state.m_speed);
        ImGui::Text("Best predicted steering: %.3f", best.predicted_state.m_steering_angle);
        ImGui::Text("Best predicted steering velocity: %.3f", best.predicted_state.m_steering_angle_velocity);

        if (ImGui::TreeNode("Top local candidates")) {
            const size_t display_count = std::min<size_t>(candidates.size(), 8);
            for (size_t i = 0; i < display_count; i++) {
                const Vehicle::SimulationControlCandidate& candidate = candidates[i];
                const Vehicle::SimulationControlCandidateDebug& candidate_debug = candidate.debug;
                ImGui::Text(
                    "#%zu loss=%.2f acc=%.2f steer_acc=%.2f len=%.2f s=%.2f->%.2f target=%.2f err=%.2f",
                    i,
                    candidate.loss,
                    candidate.control_command.speed_acceleration,
                    candidate.control_command.steer_acceleration,
                    candidate_debug.trajectory_length,
                    candidate_debug.start_s,
                    candidate_debug.end_s,
                    candidate_debug.target_end_s,
                    candidate_debug.target_end_dist
                );
            }
            ImGui::TreePop();
        }
    }

    ImGui::Separator();
    if (ImGui::TreeNode("Local planner follow params")) {
        Vehicle::SimulationFollowParams& params = m_vehicle.follow_params();

        float cruise_speed = params.cruise_speed;
        if (ImGui::DragFloat("Cruise speed", &cruise_speed, 0.1f, 0.0f, 40.0f, "%.3f")) {
            set_car_speed(cruise_speed);
        }
        if (ImGui::DragFloat("Slowdown distance from path", &params.slowdown_distance_from_path, 0.05f, 0.0f, 50.0f, "%.3f")) {
            params.slowdown_distance_from_path = std::max(0.0f, params.slowdown_distance_from_path);
        }
        if (ImGui::DragFloat("Min off-path speed factor", &params.min_off_path_speed_factor, 0.01f, 0.0f, 1.0f, "%.3f")) {
            params.min_off_path_speed_factor = std::clamp(params.min_off_path_speed_factor, 0.0f, 1.0f);
        }
        if (ImGui::DragFloat("Projection backtrack window", &params.projection_backtrack_window, 0.05f, 0.0f, 20.0f, "%.3f")) {
            params.projection_backtrack_window = std::max(0.0f, params.projection_backtrack_window);
        }
        if (ImGui::DragFloat("Projection lookahead base", &params.projection_lookahead_base, 0.05f, 0.0f, 50.0f, "%.3f")) {
            params.projection_lookahead_base = std::max(0.0f, params.projection_lookahead_base);
        }
        if (ImGui::DragFloat("Min virtual direction segment length", &params.min_direction_segment_virtual_length, 0.05f, 0.0f, 20.0f, "%.3f")) {
            params.min_direction_segment_virtual_length = std::max(0.0f, params.min_direction_segment_virtual_length);
        }
        if (ImGui::DragFloat("Direction switch arrival distance", &params.direction_switch_arrival_distance, 0.01f, 0.0f, 5.0f, "%.3f")) {
            params.direction_switch_arrival_distance = std::max(0.0f, params.direction_switch_arrival_distance);
        }
        if (ImGui::DragFloat("Direction switch arrival speed", &params.direction_switch_arrival_speed, 0.01f, 0.0f, 5.0f, "%.3f")) {
            params.direction_switch_arrival_speed = std::max(0.0f, params.direction_switch_arrival_speed);
        }
        if (ImGui::DragFloat("Direction switch approach speed", &params.direction_switch_approach_speed, 0.01f, 0.0f, 5.0f, "%.3f")) {
            params.direction_switch_approach_speed = std::max(0.0f, params.direction_switch_approach_speed);
        }

        if (ImGui::Button("Reset follow params")) {
            params.cruise_speed = std::max(0.0f, m_desc.vehicle_cruise_speed);
            params.slowdown_distance_from_path = std::max(0.0f, m_desc.vehicle_slowdown_distance_from_path);
            params.min_off_path_speed_factor = std::clamp(
                m_desc.vehicle_min_off_path_speed_factor,
                0.0f,
                1.0f
            );
            params.projection_backtrack_window = std::max(0.0f, m_desc.vehicle_projection_backtrack_window);
            params.projection_lookahead_base = std::max(0.0f, m_desc.vehicle_projection_lookahead_base);
            params.min_direction_segment_virtual_length =
                std::max(0.0f, m_desc.vehicle_min_direction_segment_virtual_length);
            params.direction_switch_arrival_distance =
                std::max(0.0f, m_desc.vehicle_direction_switch_arrival_distance);
            params.direction_switch_arrival_speed =
                std::max(0.0f, m_desc.vehicle_direction_switch_arrival_speed);
            params.direction_switch_approach_speed =
                std::max(0.0f, m_desc.vehicle_direction_switch_approach_speed);
            m_car_speed = params.cruise_speed;
        }

        ImGui::TreePop();
    }

    ImGui::Separator();
    if (ImGui::TreeNode("Local planner loss weights")) {
        Vehicle::SimulationLossWeights& weights = m_vehicle.loss_weights();

        auto drag_non_negative_float = [](const char* label, float& value, float speed, float max_value) {
            if (ImGui::DragFloat(label, &value, speed, 0.0f, max_value, "%.3f")) {
                value = std::max(0.0f, value);
            }
        };

        drag_non_negative_float("Position", weights.position, 0.05f, 100.0f);
        drag_non_negative_float("Heading", weights.heading, 0.01f, 20.0f);
        drag_non_negative_float("Speed", weights.speed, 0.01f, 20.0f);
        drag_non_negative_float("Progress tracking", weights.progress_tracking, 0.01f, 50.0f);
        drag_non_negative_float("Forward progress", weights.forward_progress, 0.01f, 20.0f);
        drag_non_negative_float("Backward motion", weights.backward_progress, 0.05f, 100.0f);
        drag_non_negative_float("Steering", weights.steering, 0.01f, 20.0f);
        drag_non_negative_float("Control", weights.control, 0.001f, 5.0f);
        drag_non_negative_float("Steering rate", weights.steering_rate, 0.01f, 20.0f);

        if (ImGui::Button("Reset loss weights"))
            m_vehicle.reset_loss_weights();

        ImGui::TreePop();
    }
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
    const bool cleanup_revision_changed =
        m_path_direction_cleanup_revision != m_synced_path_direction_cleanup_revision;
    if (result.generation == m_synced_path_generation && !cleanup_revision_changed)
        return;

    cleanup_short_direction_runs(
        result.nonholonomic_astar_path,
        m_desc.global_path_direction_cleanup_min_segment_length
    );
    if (cleanup_revision_changed)
        m_local_planner.set_astar_path(result.nonholonomic_astar_path);
    else
        m_local_planner.set_astar_path(result.nonholonomic_astar_path, result.generation);

    std::lock_guard<std::mutex> lock(m_path_mutex);
    plain_astar_path = std::move(result.plain_astar_path);
    nonholonomic_astar_path = std::move(result.nonholonomic_astar_path);
    explored_paths = std::move(result.explored_paths);
    unimpended_path = std::move(result.unimpended_path);
    total_path_finding_time = result.total_path_finding_time;
    m_synced_path_generation = result.generation;
    m_synced_path_direction_cleanup_revision = m_path_direction_cleanup_revision;
    current_target_path_point_id = 0;
}

Transform Celeris::rear_axle_transform_from_lidar_transform(const Transform& lidar_transform) const {
    Transform rear_axle_transform = lidar_transform;
    const glm::quat scan_rotation = glm::normalize(lidar_transform.rotation);
    const glm::quat lidar_mount_rotation = glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat model_rotation = glm::normalize(scan_rotation * glm::inverse(lidar_mount_rotation));

    const glm::vec3 rear_axle_offset = rear_axle_midpoint_offset();
    const glm::vec3 lidar_model_offset = lidar_offset();

    rear_axle_transform.rotation = scan_rotation;
    rear_axle_transform.position =
        lidar_transform.position +
        model_rotation * ((rear_axle_offset - lidar_model_offset) * lidar_transform.scale);

    return rear_axle_transform;
}

Transform Celeris::lidar_transform_from_rear_axle_transform(const Transform& rear_axle_transform) const {
    Transform lidar_transform = rear_axle_transform;
    const glm::quat scan_rotation = glm::normalize(rear_axle_transform.rotation);
    const glm::quat lidar_mount_rotation = glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::quat model_rotation = glm::normalize(scan_rotation * glm::inverse(lidar_mount_rotation));

    const glm::vec3 rear_axle_offset = rear_axle_midpoint_offset();
    const glm::vec3 lidar_model_offset = lidar_offset();

    lidar_transform.rotation = scan_rotation;
    lidar_transform.position =
        rear_axle_transform.position -
        model_rotation * ((rear_axle_offset - lidar_model_offset) * rear_axle_transform.scale);

    return lidar_transform;
}

glm::vec3 Celeris::rear_axle_midpoint_offset() const {
    return vehicle_geometry_position_to_model_offset(
        m_desc.vehicle_geometry,
        m_desc.vehicle_geometry.rear_axle_midpoint()
    );
}

glm::vec3 Celeris::lidar_offset() const {
    return vehicle_geometry_position_to_model_offset(
        m_desc.vehicle_geometry,
        m_desc.vehicle_geometry.lidar_position()
    );
}

bool Celeris::find_closest_next_path_point(uint32_t current_id, uint32_t& output_id, uint32_t& output_dist) {
    if (current_id + 1 >= nonholonomic_astar_path.size())
        return false;

    uint32_t best_id = current_id + 1;
    float best_dist = glm::distance(m_start_position.pos, nonholonomic_astar_path[best_id].pos);

    for (uint32_t i = current_id + 2; i < nonholonomic_astar_path.size(); i++) {
        const float dist = glm::distance(m_start_position.pos, nonholonomic_astar_path[i].pos);
        if (dist < best_dist) {
            best_id = i;
            best_dist = dist;
        }
    }

    output_id = best_id;
    output_dist = static_cast<uint32_t>(std::round(best_dist));
    return true;
};
