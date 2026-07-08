#include "celeris.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <imgui.h>
#include <iostream>
#include <limits>
#include <unordered_set>
#include <utility>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include <glm/gtc/constants.hpp>
#include "../path_utils.h"
#include "../renderer/point_cloud/point_instance.h"
#include "../vulkan_self/vulkan_command_buffer.h"
#include "../vulkan_self/vulkan_device.h"
#include "../vulkan_self/vulkan_fence.h"
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

    struct IVec3Hash {
        size_t operator()(const glm::ivec3& value) const noexcept {
            size_t seed = 0u;
            auto combine = [&](int component) {
                const size_t h = std::hash<int>{}(component);
                seed ^= h + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
            };
            combine(value.x);
            combine(value.y);
            combine(value.z);
            return seed;
        }
    };

    struct IVec3Equal {
        bool operator()(const glm::ivec3& a, const glm::ivec3& b) const noexcept {
            return a.x == b.x && a.y == b.y && a.z == b.z;
        }
    };

    glm::ivec3 lerp_color(glm::vec3 a, glm::vec3 b, float t) noexcept {
        const glm::vec3 color = a + (b - a) * std::clamp(t, 0.0f, 1.0f);
        return glm::ivec3{
            static_cast<int>(std::lround(color.x)),
            static_cast<int>(std::lround(color.y)),
            static_cast<int>(std::lround(color.z))
        };
    }

    glm::ivec3 path_potential_color(float normalized) noexcept {
        normalized = std::clamp(normalized, 0.0f, 1.0f);
        if (normalized < 0.33f) {
            return lerp_color(
                glm::vec3{20.0f, 120.0f, 255.0f},
                glm::vec3{60.0f, 220.0f, 140.0f},
                normalized / 0.33f
            );
        }
        if (normalized < 0.66f) {
            return lerp_color(
                glm::vec3{60.0f, 220.0f, 140.0f},
                glm::vec3{255.0f, 218.0f, 68.0f},
                (normalized - 0.33f) / 0.33f
            );
        }
        return lerp_color(
            glm::vec3{255.0f, 218.0f, 68.0f},
            glm::vec3{255.0f, 58.0f, 48.0f},
            (normalized - 0.66f) / 0.34f
        );
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

    bool finite_vec3(glm::vec3 value) noexcept {
        return std::isfinite(value.x) &&
               std::isfinite(value.y) &&
               std::isfinite(value.z);
    }

    uint32_t probe_count_for_range(float min_value, float max_value, float step) noexcept {
        const float range = std::max(max_value - min_value, 0.0f);
        return std::max(1u, static_cast<uint32_t>(std::floor(range / step)) + 1u);
    }

    float probe_coordinate(float min_value, float max_value, uint32_t index, uint32_t count) noexcept {
        if (count <= 1u) {
            return (min_value + max_value) * 0.5f;
        }

        const float t = static_cast<float>(index) / static_cast<float>(count - 1u);
        return glm::mix(min_value, max_value, t);
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

    std::filesystem::path resolve_celeris_file_path(const std::filesystem::path& path) {
        if (path.is_absolute()) {
            return path;
        }

#ifdef CELERIS_SOURCE_DIR
        const std::filesystem::path source_root = CELERIS_SOURCE_DIR;
        if (std::filesystem::exists(source_root)) {
            return source_root / path;
        }
#endif

        return path_utils::executable_dir() / path;
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
        m_waypoint_path(engine, manager_bundle.mesh_manager(), material_instance_manager),
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
    logger().check(desc.waypoint_reach_radius > 0.0f, "Waypoint reach radius must be greater than 0");

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
    follow_params.segment_switch_radius =
        std::max(0.0f, desc.vehicle_segment_switch_radius);
    follow_params.min_direction_segment_virtual_length =
        std::max(0.0f, desc.vehicle_min_direction_segment_virtual_length);
    follow_params.direction_switch_arrival_speed =
        std::max(0.0f, desc.vehicle_direction_switch_arrival_speed);
    follow_params.direction_switch_approach_speed =
        std::max(0.0f, desc.vehicle_direction_switch_approach_speed);
    m_car_speed = follow_params.cruise_speed;

    m_waypoint_reach_radius = desc.waypoint_reach_radius;
    m_gamepad_commands_enabled = desc.gamepad_commands_enabled;
    m_gamepad_command = desc.gamepad_command;
    if (m_gamepad_commands_enabled)
        m_command_sender.set_command(m_gamepad_command);
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
    // m_command_sender.start();
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
    if (m_gamepad_commands_enabled) {
        const float delta_time = m_local_planner.calculate_delta_time();
        if (delta_time > 0.0f) {
            m_vehicle.state().m_steering_angle_velocity =
                m_gamepad_command.steering_angle_velocity;
            m_vehicle.simulate_vehicle(
                m_gamepad_command.acceleration,
                0.0f,
                delta_time,
                std::min(delta_time, 0.05f),
                false
            );
        }
    } else {
        m_local_planner.predict_vehicle_state(m_vehicle);
    }

    float vehicle_height = m_vehicle_position.pos.y;

    if (auto scan = m_scan_receiver.try_pop_scan(*m_manager_bundle)) {
        glm::vec3 raw_position = scan->point_cloud().transform.position;
        glm::quat raw_rotation = glm::normalize(scan->point_cloud().transform.rotation);

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
            if (m_has_start_lidar_scan_position) {
                m_network_scan->point_cloud().transform.position = m_start_lidar_scan_position;
                m_network_scan->point_cloud().transform.rotation = m_has_start_lidar_scan_rotation
                    ? m_start_lidar_scan_rotation
                    : raw_rotation;
            } else {
                m_network_scan->point_cloud().transform.position = glm::vec3(0.0f);
                m_network_scan->point_cloud().transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            }

            m_has_previous_lidar_pose = true;
        } else {
            if (glm::dot(m_previous_lidar_rotation, raw_rotation) < 0.0f)
                raw_rotation = -raw_rotation;

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
        }

        // if (!m_needs_map_localization && m_voxel_point_map.map_point_count() > 0u) {
        if (m_voxel_point_map.map_point_count() > 0u) {
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

        const glm::quat vehicle_rotation = glm::normalize(vehicle_transform.rotation);
        const glm::vec3 vehicle_forward = vehicle_rotation * glm::vec3(-1.0f, 0.0f, 0.0f);
        const float vehicle_heading = std::atan2(vehicle_forward.z, vehicle_forward.x);

        // Корректируем позу машины по lidar/GICP. Скорость и руль приходят отдельно.
        m_vehicle.state().m_position = glm::vec2{
            vehicle_transform.position.x,
            vehicle_transform.position.z
        };
        m_vehicle.state().m_heading = vehicle_heading;

        // //=====================
        m_voxel_map_inserter.insert(m_voxel_point_map, m_network_scan->point_cloud(), m_network_scan->normal_buffer());
        m_voxel_grid->voxelize_point_cloud(
            *m_engine,
            m_network_scan->point_cloud(),
            m_network_scan->normal_buffer(),
            voxel_write_list,
            m_desc.max_write_count
        );
        // //=====================

        const glm::vec3 collision_raw_position = vehicle_pose.pos;

        collision(
            m_collision_raw_position_history,
            vehicle_pose.pos
        );
        remember_collision_raw_position(collision_raw_position);

        sync_vehicle_position_from_state(vehicle_height);
        update_waypoint_navigation();

        if (!m_waypoint_path_completed && has_planned_path() && is_path_impended(submit_context))
            request_path_replan();

        m_previous_lidar_position = raw_position;
        m_previous_lidar_rotation = raw_rotation;
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
        VehicleCommand vehicle_command;
        if (!m_waypoint_path_completed || m_waypoint_path.waypoints().empty()) {
            vehicle_command = m_local_planner.step(
                m_vehicle,
                m_path_intersection_detector,
                submit_context
            );
        }

        if (!m_gamepad_commands_enabled)
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

void Celeris::set_start_lidar_scan_position(glm::vec3 position) noexcept {
    m_has_start_lidar_scan_position = true;
    m_has_start_lidar_scan_rotation = false;
    m_start_lidar_scan_position = position;

    if (m_has_map_bounding_box && m_voxel_point_map.map_point_count() > 0u) {
        m_needs_map_localization = false;
    }
}

void Celeris::set_start_lidar_scan_position(const NonholonomicPos& position) noexcept {
    set_start_lidar_scan_position(position.pos);
    m_has_start_lidar_scan_rotation = true;
    m_start_lidar_scan_rotation = glm::angleAxis(
        glm::pi<float>() - position.theta,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
}

void Celeris::set_car_speed(float speed) noexcept {
    if (!std::isfinite(speed))
        return;

    m_car_speed = std::max(0.0f, speed);
    m_vehicle.follow_params().cruise_speed = m_car_speed;
}

void Celeris::set_waypoint_reach_radius(float radius) noexcept {
    if (std::isfinite(radius) && radius > 0.0f)
        m_waypoint_reach_radius = radius;
}

void Celeris::set_gamepad_commands_enabled(bool enabled) noexcept {
    m_gamepad_commands_enabled = enabled;
    if (enabled) {
        m_command_sender.set_command(m_gamepad_command);
    } else {
        m_gamepad_command = VehicleCommand{};
    }
}

void Celeris::set_gamepad_command(VehicleCommand command) noexcept {
    if (!std::isfinite(command.acceleration) || !std::isfinite(command.steering_angle_velocity))
        return;

    m_gamepad_command = command;
    if (m_gamepad_commands_enabled)
        m_command_sender.set_command(m_gamepad_command);
}

void Celeris::add_waypoint(glm::vec3 position) {
    m_waypoint_path.add_waypoint(position);
    reset_waypoint_navigation();
}

void Celeris::add_waypoint(const NonholonomicPos& position) {
    m_waypoint_path.add_waypoint(position);
    reset_waypoint_navigation();
}

void Celeris::delete_last_waypoint() {
    m_waypoint_path.delete_last_waypoint();
    reset_waypoint_navigation();
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

float Celeris::vehicle_speed() const noexcept {
    return m_vehicle.state().m_speed;
}

float Celeris::vehicle_steering_angle() const noexcept {
    return m_vehicle.state().m_steering_angle;
}

float Celeris::waypoint_reach_radius() const noexcept {
    return m_waypoint_reach_radius;
}

bool Celeris::gamepad_commands_enabled() const noexcept {
    return m_gamepad_commands_enabled;
}

VehicleCommand Celeris::gamepad_command() const noexcept {
    return m_gamepad_command;
}

bool Celeris::command_sender_running() const noexcept {
    return m_command_sender.is_running();
}

bool Celeris::command_sender_connected() const noexcept {
    return m_command_sender.is_connected();
}

size_t Celeris::active_waypoint_index() const noexcept {
    return m_active_waypoint_index;
}

bool Celeris::waypoint_path_completed() const noexcept {
    return m_waypoint_path_completed;
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

float Celeris::local_planner_segment_switch_radius() const noexcept {
    return m_vehicle.follow_params().segment_switch_radius;
}

Footprint& Celeris::footprint() noexcept {
    return m_path_planner.footprint();
}

VoxelGrid* Celeris::voxel_grid() noexcept {
    return m_voxel_grid;
}

WaypointPath& Celeris::waypoint_path() noexcept {
    return m_waypoint_path;
}

const WaypointPath& Celeris::waypoint_path() const noexcept {
    return m_waypoint_path;
}

uint32_t Celeris::received_scan_count() const noexcept {
    return m_received_scan_count;
}

const std::vector<Celeris::Waypoint>& Celeris::waypoints() const noexcept {
    return m_waypoint_path.waypoints();
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
    if (m_waypoint_path_completed && !m_waypoint_path.waypoints().empty()) {
        reset_waypoint_navigation();
    }

    NonholonomicPos goal = m_goal_position;
    NonholonomicPos start = m_start_position;

    if (active_waypoint_goal_pose(goal)) {
        start = m_vehicle_position;
        m_goal_position = goal;
    } else if (!m_has_start_position || !m_has_goal_position) {
        return false;
    }

    return request_grounded_path_replan(start, goal);
}

bool Celeris::request_grounded_path_replan(NonholonomicPos start, NonholonomicPos goal) {
    const NonholonomicPos requested_start = start;
    const NonholonomicPos requested_goal = goal;
    const bool allow_flying = m_desc.nonholonomic_astar_desc.allow_flying_over_precipices;

    const bool start_adjusted = m_path_planner.request_adjust_to_ground(
        start.pos,
        500,
        500,
        -1,
        allow_flying
    );
    const bool goal_adjusted = m_path_planner.request_adjust_to_ground(
        goal.pos,
        500,
        500,
        -1,
        allow_flying
    );

    if (!start_adjusted || !goal_adjusted) {
        if (!allow_flying) {
            logger().log_error()
                << "Failed to adjust path endpoints to ground. "
                << "start_adjusted=" << (start_adjusted ? "true" : "false")
                << " goal_adjusted=" << (goal_adjusted ? "true" : "false") << "\n";
            return false;
        }

        logger().log_error()
            << "Failed to adjust path endpoints to ground; using requested positions because flying is allowed. "
            << "start_adjusted=" << (start_adjusted ? "true" : "false")
            << " goal_adjusted=" << (goal_adjusted ? "true" : "false") << "\n";

        if (!start_adjusted)
            start = requested_start;
        if (!goal_adjusted)
            goal = requested_goal;
    }

    m_path_planner.request_path_replan(start, goal);
    return true;
}

void Celeris::reset_local_planner_tracking() {
    std::vector<NonholonomicPos> current_path;
    {
        std::lock_guard<std::mutex> lock(m_path_mutex);
        current_path = nonholonomic_astar_path;
    }

    if (!current_path.empty())
        m_local_planner.set_astar_path(current_path);

    m_local_planner.reset_tracking();
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
        "Local path segment: %zu / %zu",
        m_local_planner.active_path_segment_index(),
        m_local_planner.path_segment_count()
    );
    ImGui::Text(
        "Local path generation: %llu",
        static_cast<unsigned long long>(m_local_planner.path_generation())
    );
    if (ImGui::TreeNode("Active path potential visualization")) {
        if (ImGui::DragFloat("Field radius", &m_path_potential_visualization_radius, 0.5f, 1.0f, 100.0f, "%.2f")) {
            m_path_potential_visualization_radius =
                std::max(1.0f, m_path_potential_visualization_radius);
        }
        if (ImGui::DragFloat("Field step", &m_path_potential_visualization_step, 0.1f, 0.25f, 10.0f, "%.2f")) {
            m_path_potential_visualization_step =
                std::max(0.25f, m_path_potential_visualization_step);
        }
        if (ImGui::DragFloat("Vertical drop", &m_path_potential_visualization_vertical_drop, 0.5f, 0.0f, 100.0f, "%.2f")) {
            m_path_potential_visualization_vertical_drop =
                std::max(0.0f, m_path_potential_visualization_vertical_drop);
        }
        float distance_exponent = m_vehicle.path_potential_distance_exponent();
        if (ImGui::DragFloat("Distance exponent", &distance_exponent, 0.01f, 0.0f, 4.0f, "%.3f")) {
            m_vehicle.set_path_potential_distance_exponent(distance_exponent);
        }
        if (ImGui::Button("Visualize active path potential")) {
            visualize_active_path_potential();
        }
        ImGui::Text(
            "Potential voxels: %zu",
            m_last_path_potential_visualization_voxel_count
        );
        ImGui::TreePop();
    }
    if (!candidates.empty()) {
        auto display_candidate_loss_breakdown =
            [](const Vehicle::SimulationLossBreakdown& breakdown) {
                ImGui::Text(
                    "    loss parts: pos=%+.2f head=%+.2f speed=%+.2f prog=%+.2f steer=%+.2f ctrl=%+.2f sum=%+.2f",
                    breakdown.position,
                    breakdown.heading,
                    breakdown.speed,
                    breakdown.progress,
                    breakdown.steering,
                    breakdown.control,
                    breakdown.total()
                );
            };

        auto display_candidate_summary =
            [&](size_t i, const Vehicle::SimulationControlCandidate& candidate) {
                const Vehicle::SimulationControlCandidateDebug& candidate_debug = candidate.debug;
                ImGui::Text(
                    "#%zu loss=%.2f acc=%.2f steer_acc=%.2f len=%.2f s=%.2f->%.2f target=%.2f err=%.2f v=%.2f",
                    i,
                    candidate.loss,
                    candidate.control_command.speed_acceleration,
                    candidate.control_command.steer_acceleration,
                    candidate_debug.trajectory_length,
                    candidate_debug.start_s,
                    candidate_debug.end_s,
                    candidate_debug.target_end_s,
                    candidate_debug.target_end_dist,
                    candidate.predicted_state.m_speed
                );
                display_candidate_loss_breakdown(candidate_debug.loss_breakdown);
            };

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
        display_candidate_loss_breakdown(debug.loss_breakdown);

        if (ImGui::TreeNode("Top local candidates")) {
            const size_t display_count = std::min<size_t>(candidates.size(), 8);
            for (size_t i = 0; i < display_count; i++) {
                display_candidate_summary(i, candidates[i]);
            }
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("All local candidates")) {
            for (size_t i = 0; i < candidates.size(); i++) {
                display_candidate_summary(i, candidates[i]);
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
        if (ImGui::DragFloat("Segment switch radius", &params.segment_switch_radius, 0.01f, 0.0f, 10.0f, "%.3f")) {
            params.segment_switch_radius = std::max(0.0f, params.segment_switch_radius);
        }
        if (ImGui::DragFloat("Min virtual direction segment length", &params.min_direction_segment_virtual_length, 0.05f, 0.0f, 20.0f, "%.3f")) {
            params.min_direction_segment_virtual_length = std::max(0.0f, params.min_direction_segment_virtual_length);
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
            params.segment_switch_radius = std::max(0.0f, m_desc.vehicle_segment_switch_radius);
            params.min_direction_segment_virtual_length =
                std::max(0.0f, m_desc.vehicle_min_direction_segment_virtual_length);
            params.direction_switch_arrival_speed =
                std::max(0.0f, m_desc.vehicle_direction_switch_arrival_speed);
            params.direction_switch_approach_speed =
                std::max(0.0f, m_desc.vehicle_direction_switch_approach_speed);
            m_car_speed = params.cruise_speed;
        }

        ImGui::TreePop();
    }

    ImGui::Separator();
    if (ImGui::TreeNode("Local planner potential weights")) {
        Vehicle::SimulationLossWeights& weights = m_vehicle.loss_weights();

        auto drag_non_negative_float = [](const char* label, float& value, float speed, float max_value) {
            if (ImGui::DragFloat(label, &value, speed, 0.0f, max_value, "%.3f")) {
                value = std::max(0.0f, value);
            }
        };

        drag_non_negative_float("Attach position", weights.position, 0.05f, 100.0f);
        drag_non_negative_float("Attach heading", weights.heading, 0.01f, 20.0f);
        drag_non_negative_float("Speed recovery", weights.speed, 0.01f, 20.0f);
        drag_non_negative_float("Remaining path", weights.progress_tracking, 0.01f, 50.0f);
        drag_non_negative_float("Steering state", weights.steering, 0.01f, 20.0f);
        drag_non_negative_float("Control effort", weights.control, 0.001f, 5.0f);
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

void Celeris::visualize_active_path_potential() {
    logger().check(m_engine, "Engine was null");
    logger().check(m_voxel_grid, "Voxel grid was null");

    struct PathPotentialSample {
        glm::ivec3 voxel_pos;
        float potential = 0.0f;
    };

    const std::vector<VehiclePathPoint>& path = m_local_planner.vehicle_path();
    const Vehicle::PathArcLengthTable& path_arc_lengths =
        m_local_planner.vehicle_path_arc_lengths();
    const float active_segment_min_s = m_local_planner.path_window_min_s();
    const float active_segment_max_s = m_local_planner.path_window_max_s();

    const size_t max_write_count = static_cast<size_t>(m_desc.max_write_count);
    const size_t max_sample_count = std::max<size_t>(1u, max_write_count / 2u);

    std::vector<PathPotentialSample> samples;
    std::unordered_set<glm::ivec3, IVec3Hash, IVec3Equal> sample_voxels;

    if (path.size() >= 2u &&
        path_arc_lengths.point_s.size() == path.size() &&
        active_segment_max_s > active_segment_min_s + Utils::eps)
    {
        const glm::vec3 voxel_size = m_voxel_grid->voxel_size();
        const float min_voxel_size = std::max(min_component(voxel_size), Utils::eps);
        const float radius = std::max(0.0f, m_path_potential_visualization_radius);
        const float step =
            std::max(min_voxel_size, m_path_potential_visualization_step);
        const float vertical_drop =
            std::max(0.0f, m_path_potential_visualization_vertical_drop);
        const int voxel_y =
            static_cast<int>(std::floor((m_vehicle_position.pos.y - vertical_drop) / voxel_size.y));

        const std::vector<Vehicle::SimulationControlCandidate>& candidates =
            m_local_planner.last_simulation_candidates();
        const Vehicle::VehicleControlCommand visualization_control =
            candidates.empty()
                ? Vehicle::VehicleControlCommand{}
                : candidates.front().control_command;

        Vehicle::VehicleTransformState sampled_state = m_vehicle.state();
        float min_potential = std::numeric_limits<float>::infinity();
        float max_potential = -std::numeric_limits<float>::infinity();

        for (float dx = -radius; dx <= radius + Utils::eps; dx += step) {
            for (float dz = -radius; dz <= radius + Utils::eps; dz += step) {
                if (samples.size() >= max_sample_count)
                    break;

                const glm::vec3 world_pos{
                    m_vehicle_position.pos.x + dx,
                    m_vehicle_position.pos.y - vertical_drop,
                    m_vehicle_position.pos.z + dz
                };
                const glm::ivec3 voxel_pos{
                    static_cast<int>(std::floor(world_pos.x / voxel_size.x)),
                    voxel_y,
                    static_cast<int>(std::floor(world_pos.z / voxel_size.z))
                };
                if (sample_voxels.find(voxel_pos) != sample_voxels.end())
                    continue;

                sampled_state.m_position = glm::vec2{world_pos.x, world_pos.z};
                const float potential = m_vehicle.evaluate_path_potential(
                    sampled_state,
                    path,
                    path_arc_lengths,
                    active_segment_min_s,
                    active_segment_max_s,
                    visualization_control.speed_acceleration,
                    visualization_control.steer_acceleration
                );
                if (!std::isfinite(potential))
                    continue;

                sample_voxels.insert(voxel_pos);
                min_potential = std::min(min_potential, potential);
                max_potential = std::max(max_potential, potential);
                samples.push_back(PathPotentialSample{
                    .voxel_pos = voxel_pos,
                    .potential = potential
                });
            }
            if (samples.size() >= max_sample_count)
                break;
        }

        const float potential_range = max_potential - min_potential;
        if (potential_range <= Utils::eps) {
            for (PathPotentialSample& sample : samples) {
                sample.potential = 0.0f;
            }
        } else {
            for (PathPotentialSample& sample : samples) {
                sample.potential = (sample.potential - min_potential) / potential_range;
            }
        }
    }

    std::vector<VoxelWriteGPU> voxel_writes;
    voxel_writes.reserve(
        std::min(
            max_write_count,
            m_path_potential_visualization_voxels.size() + samples.size()
        )
    );

    for (const glm::ivec3& voxel_pos : m_path_potential_visualization_voxels) {
        if (voxel_writes.size() >= max_write_count)
            break;
        if (sample_voxels.find(voxel_pos) != sample_voxels.end())
            continue;

        voxel_writes.push_back(VoxelWriteGPU{
            .world_voxel = glm::ivec4(voxel_pos, 0),
            .voxel_data = VoxelDataGPU(0u, 0u, 0u),
            .set_flags = OVERWRITE_BIT
        });
    }

    for (const PathPotentialSample& sample : samples) {
        if (voxel_writes.size() >= max_write_count)
            break;

        voxel_writes.push_back(VoxelWriteGPU{
            .world_voxel = glm::ivec4(sample.voxel_pos, 0),
            .voxel_data = VoxelDataGPU(
                1u,
                VOXEL_VISABILITY_FLAG_BIT | VOXEL_EASY_OVERWRITE_FLAG_BIT,
                path_potential_color(sample.potential)
            ),
            .set_flags = OVERWRITE_BIT
        });
    }

    if (!voxel_writes.empty()) {
        VulkanBuffer path_potential_voxel_write_list =
            VulkanBuffer::create_host_visible_storage_buffer(
                *m_engine,
                sizeof(uint32_t) * 4u + sizeof(VoxelWriteGPU) * voxel_writes.size()
            );
        const uint32_t voxel_write_count =
            static_cast<uint32_t>(voxel_writes.size());
        path_potential_voxel_write_list.upload_scalar<uint32_t>(voxel_write_count, 0u);
        path_potential_voxel_write_list.upload(voxel_writes, sizeof(uint32_t) * 4u);

        VulkanCommandBuffer compute_command_buffer(
            m_engine->device(),
            m_engine->compute_command_pool()
        );
        {
            auto scope = compute_command_buffer.begin_scope();
            m_voxel_grid->set_voxels(compute_command_buffer, path_potential_voxel_write_list);
        }
        VulkanFence compute_fence(m_engine->device());
        m_engine->compute_submit(compute_command_buffer, &compute_fence);
        compute_fence.wait();
    }

    m_path_potential_visualization_voxels.clear();
    m_path_potential_visualization_voxels.reserve(samples.size());
    for (const PathPotentialSample& sample : samples) {
        m_path_potential_visualization_voxels.push_back(sample.voxel_pos);
    }
    m_last_path_potential_visualization_voxel_count = samples.size();
}

void Celeris::sync_point_map_and_voxel_grid() {
    m_voxel_grid->voxelize_point_cloud(
            *m_engine,
            m_voxel_point_map.map_point_buffer,
            m_voxel_point_map.map_normal_buffer,
            m_voxel_point_map.map_point_count(),
            voxel_write_list,
            m_desc.max_write_count
        );
    // m_voxel_grid->voxelize_point_map(
    //     m_voxel_point_map,
    //     voxel_write_list,
    //     m_engine->compute_command_pool(),
    //     m_engine->compute_queue()
    // );
}

void Celeris::save_map(const std::filesystem::path& path) {
    std::filesystem::path resolved_path = resolve_celeris_file_path(path);
    resolved_path.replace_extension(".vpm");

    const std::filesystem::path parent_path = resolved_path.parent_path();

    if (!parent_path.empty()) {
        std::filesystem::create_directories(parent_path);
    }

    m_voxel_point_map.save(resolved_path);
}

void Celeris::load_map(const std::filesystem::path& path) {
    m_voxel_point_map.load(resolve_celeris_file_path(path));
    update_map_bounding_box();
    m_needs_map_localization =
        m_has_map_bounding_box &&
        m_voxel_point_map.map_point_count() > 0u &&
        !m_has_start_lidar_scan_position;
    sync_point_map_and_voxel_grid();
}

void Celeris::save_waypoint_path(const std::filesystem::path& path) {
    std::filesystem::path resolved_path = resolve_celeris_file_path(path);
    resolved_path.replace_extension(".wpp");

    const std::filesystem::path parent_path = resolved_path.parent_path();

    if (!parent_path.empty()) {
        std::filesystem::create_directories(parent_path);
    }

    m_waypoint_path.save(resolved_path);
}

void Celeris::load_waypoint_path(const std::filesystem::path& path) {
    m_waypoint_path.load(resolve_celeris_file_path(path));
    reset_waypoint_navigation();
}

bool Celeris::localize_on_map() {
    if (!m_network_scan) {
        std::cout << "Localizing on map failed: no LiDAR scan is loaded" << std::endl;
        return false;
    }

    if (!m_has_map_bounding_box || m_voxel_point_map.map_point_count() == 0u) {
        std::cout << "Localizing on map failed: no loaded map bounds are available" << std::endl;
        return false;
    }

    PointCloud& source_point_cloud = m_network_scan->point_cloud();

    const glm::vec3 original_position = source_point_cloud.transform.position;
    const glm::quat original_rotation = glm::normalize(source_point_cloud.transform.rotation);

    const glm::quat probe_rotation = m_received_scan_count > 0u
        ? glm::normalize(m_previous_lidar_rotation)
        : original_rotation;

    const glm::vec3 bounds_min(m_map_bounding_box.min);
    const glm::vec3 bounds_max(m_map_bounding_box.max);

    float probe_step = std::max(m_desc.localization_probe_step, 0.1f);
    uint32_t x_count = probe_count_for_range(bounds_min.x, bounds_max.x, probe_step);
    uint32_t z_count = probe_count_for_range(bounds_min.z, bounds_max.z, probe_step);

    const uint32_t max_candidates = std::max(m_desc.localization_max_candidates, 1u);
    uint64_t candidate_count = static_cast<uint64_t>(x_count) * static_cast<uint64_t>(z_count);

    if (candidate_count > max_candidates) {
        const float spacing_scale = std::sqrt(
            static_cast<float>(candidate_count) / static_cast<float>(max_candidates)
        );
        probe_step *= spacing_scale;
        x_count = probe_count_for_range(bounds_min.x, bounds_max.x, probe_step);
        z_count = probe_count_for_range(bounds_min.z, bounds_max.z, probe_step);
    }
    candidate_count = static_cast<uint64_t>(x_count) * static_cast<uint64_t>(z_count);

    bool found_candidate = false;
    double best_rmse = std::numeric_limits<double>::infinity();
    glm::vec3 best_position = original_position;
    glm::quat best_rotation = original_rotation;

    std::cout << "Localizing on map: probing " << candidate_count
              << " candidates (" << x_count << " x " << z_count
              << ", step " << probe_step << ")" << std::endl;

    uint64_t processed_candidates = 0;
    uint32_t last_progress_percent = 0;
    auto last_progress_time = std::chrono::steady_clock::now();

    auto print_progress = [&]() {
        const uint32_t progress_percent = static_cast<uint32_t>(
            (processed_candidates * 100u) / std::max<uint64_t>(candidate_count, 1u)
        );

        const auto now = std::chrono::steady_clock::now();
        const bool should_print =
            progress_percent != last_progress_percent ||
            processed_candidates == candidate_count ||
            now - last_progress_time >= std::chrono::milliseconds(500);

        if (!should_print) {
            return;
        }

        last_progress_percent = progress_percent;
        last_progress_time = now;

        std::cout << "\rLocalizing on map: " << progress_percent << "% ("
                  << processed_candidates << "/" << candidate_count << ")";

        if (found_candidate) {
            std::cout << ", best RMSE " << best_rmse;
        }

        std::cout << std::flush;
    };

    std::cout << "Localizing on map: 0% (0/" << candidate_count << ")" << std::flush;

    for (uint32_t z_id = 0; z_id < z_count; z_id++) {
        const float z = probe_coordinate(bounds_min.z, bounds_max.z, z_id, z_count);

        for (uint32_t x_id = 0; x_id < x_count; x_id++) {
            const float x = probe_coordinate(bounds_min.x, bounds_max.x, x_id, x_count);

            source_point_cloud.transform.position = glm::vec3(x, original_position.y, z);
            source_point_cloud.transform.rotation = probe_rotation;

            const double rmse = m_gicp_pass.fit(
                m_voxel_point_map,
                source_point_cloud,
                m_network_scan->normal_buffer(),
                m_desc.localization_gicp_iterations,
                false
            );

            if (std::isfinite(rmse) && rmse < 9999.0 && rmse < best_rmse) {
                found_candidate = true;
                best_rmse = rmse;
                best_position = source_point_cloud.transform.position;
                best_rotation = glm::normalize(source_point_cloud.transform.rotation);
            }

            processed_candidates++;
            print_progress();
        }
    }

    std::cout << std::endl;

    if (!found_candidate) {
        source_point_cloud.transform.position = original_position;
        source_point_cloud.transform.rotation = original_rotation;
        std::cout << "Localizing on map failed: no valid GICP candidate found" << std::endl;
        return false;
    }

    source_point_cloud.transform.position = best_position;
    source_point_cloud.transform.rotation = best_rotation;
    m_lidar_transform = source_point_cloud.transform;
    const Transform vehicle_transform = rear_axle_transform_from_lidar_transform(m_lidar_transform);
    NonholonomicPos localized_vehicle_pose;
    localized_vehicle_pose.from_transform(vehicle_transform);
    m_vehicle.state().m_position = glm::vec2{
        localized_vehicle_pose.pos.x,
        localized_vehicle_pose.pos.z
    };
    m_vehicle.state().m_heading = localized_vehicle_pose.theta;
    sync_vehicle_position_from_state(localized_vehicle_pose.pos.y);
    if (!m_has_start_position) {
        m_start_position = m_vehicle_position;
        m_has_start_position = true;
    }
    m_needs_map_localization = false;

    std::cout << "Localizing on map succeeded: position ("
              << best_position.x << ", " << best_position.y << ", " << best_position.z
              << "), RMSE " << best_rmse << std::endl;

    return true;
}

const AABB& Celeris::get_bounding_box() const noexcept {
    return m_map_bounding_box;
}

bool Celeris::has_map_bounding_box() const noexcept {
    return m_has_map_bounding_box;
}

void Celeris::update_map_bounding_box() {
    m_has_map_bounding_box = false;
    m_map_bounding_box = AABB{
        .min = glm::vec4(0.0f),
        .max = glm::vec4(0.0f)
    };

    const uint32_t point_count = m_voxel_point_map.map_point_count();
    if (point_count == 0u) {
        return;
    }

    std::vector<PointInstance> points(point_count);
    m_voxel_point_map.map_point_buffer.read(
        points.data(),
        sizeof(PointInstance) * points.size(),
        0
    );

    glm::vec3 bounds_min(std::numeric_limits<float>::infinity());
    glm::vec3 bounds_max(-std::numeric_limits<float>::infinity());

    for (const PointInstance& point : points) {
        const glm::vec3 position(point.position);
        if (!finite_vec3(position)) {
            continue;
        }

        bounds_min = glm::min(bounds_min, position);
        bounds_max = glm::max(bounds_max, position);
        m_has_map_bounding_box = true;
    }

    if (!m_has_map_bounding_box) {
        return;
    }

    m_map_bounding_box = AABB{
        .min = glm::vec4(bounds_min, 1.0f),
        .max = glm::vec4(bounds_max, 1.0f)
    };
}

bool Celeris::is_path_impended(VulkanSubmitContext& submit_context) {
    return m_path_planner.request_is_path_impended(submit_context);
}

void Celeris::reset_waypoint_navigation() noexcept {
    m_active_waypoint_index = 0;
    m_waypoint_path_completed = false;
    is_stop_waiting = false;
    m_waypoint_path.set_first_visible_waypoint_index(0);
}

bool Celeris::has_active_waypoint() const noexcept {
    return !m_waypoint_path_completed &&
           m_active_waypoint_index < m_waypoint_path.waypoints().size();
}

NonholonomicPos Celeris::waypoint_goal_pose(size_t waypoint_index) const {
    const std::vector<Waypoint>& path = m_waypoint_path.waypoints();
    logger().check(waypoint_index < path.size(), "Waypoint index was out of range");

    const Waypoint& waypoint = path[waypoint_index];
    NonholonomicPos goal;
    goal.pos = waypoint.world_position();

    if (waypoint.directional()) {
        goal.theta = *waypoint.theta;
        return goal;
    }

    glm::vec3 heading_vector(0.0f);
    if (waypoint_index + 1 < path.size()) {
        heading_vector = path[waypoint_index + 1].world_position() - goal.pos;
    } else {
        heading_vector = goal.pos - m_vehicle_position.pos;
    }

    heading_vector.y = 0.0f;
    if (length_sq(heading_vector) <= 1e-8f) {
        goal.theta = m_vehicle_position.theta;
    } else {
        goal.theta = std::atan2(heading_vector.z, heading_vector.x);
    }

    return goal;
}

bool Celeris::active_waypoint_goal_pose(NonholonomicPos& output) const {
    if (!has_active_waypoint())
        return false;

    output = waypoint_goal_pose(m_active_waypoint_index);
    return true;
}

void Celeris::update_waypoint_navigation() {
    if (m_waypoint_path.waypoints().empty()) {
        reset_waypoint_navigation();
        return;
    }

    if (m_waypoint_path_completed) {
        m_waypoint_path.set_first_visible_waypoint_index(m_waypoint_path.waypoints().size());
        return;
    }

    m_waypoint_path.set_first_visible_waypoint_index(m_active_waypoint_index);
    bool advanced = false;

    while (has_active_waypoint()) {
        const Waypoint& waypoint = m_waypoint_path.waypoints()[m_active_waypoint_index];
        const float dist_sq = length_sq(waypoint.world_position() - m_vehicle_position.pos);
        if (dist_sq > m_waypoint_reach_radius * m_waypoint_reach_radius)
            break;

        advanced = true;
        is_stop_waiting = false;

        if (m_active_waypoint_index + 1 >= m_waypoint_path.waypoints().size()) {
            m_goal_position = waypoint_goal_pose(m_active_waypoint_index);
            m_active_waypoint_index = m_waypoint_path.waypoints().size();
            m_waypoint_path_completed = true;
            m_waypoint_path.set_first_visible_waypoint_index(m_active_waypoint_index);
            std::lock_guard<std::mutex> lock(m_path_mutex);
            current_target_path_point_id = static_cast<uint32_t>(nonholonomic_astar_path.size());
            return;
        }

        m_active_waypoint_index++;
        m_waypoint_path.set_first_visible_waypoint_index(m_active_waypoint_index);
    }

    if (!advanced || !has_active_waypoint())
        return;

    NonholonomicPos next_goal = waypoint_goal_pose(m_active_waypoint_index);
    m_goal_position = next_goal;
    {
        std::lock_guard<std::mutex> lock(m_path_mutex);
        current_target_path_point_id = 0;
    }
    request_grounded_path_replan(m_vehicle_position, next_goal);
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
