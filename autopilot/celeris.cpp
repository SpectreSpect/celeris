#include "celeris.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <imgui.h>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

#include <glm/gtc/constants.hpp>
#include "../path_utils.h"
#include "../renderer/point_cloud/point_instance.h"
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
    // m_command_sender.start();
    // m_path_planner.start(std::move(planner_submit_context));
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
        }

        if (!m_needs_map_localization && m_voxel_point_map.map_point_count() > 0u) {
            m_gicp_pass.fit(m_voxel_point_map,
                            m_network_scan->point_cloud(),
                            m_network_scan->normal_buffer(),
                            m_desc.max_gicp_iterations);
        }
        
        m_lidar_transform = m_network_scan->point_cloud().transform;
        m_start_position.from_transform(rear_axle_transform_from_lidar_transform(m_lidar_transform));

        // m_start_position.pos = m_network_scan->point_cloud().transform.position;
        // glm::quat q = glm::normalize(m_network_scan->point_cloud().transform.rotation);
        // glm::vec3 forward = q * glm::vec3(-1.0f, 0.0f, 0.0f);
        // m_start_position.theta = std::atan2(forward.z, forward.x);
        
        // start_sphere.transform.position = m_network_scan->point_cloud().transform.position;
        // start_direction_sphere.transform.position = start_pos.pos + direction_offset(start_pos.theta) * 0.85f + glm::vec3(0, 0.4f, 0);

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

        // VoxelWriteGPU blue_voxelize_prefab;
        // blue_voxelize_prefab.voxel_data = VoxelDataGPU(1, VOXEL_VISABILITY_FLAG_BIT, glm::ivec3({0, 98, 255}));
        // blue_voxelize_prefab.set_flags = OVERWRITE_BIT;

        // RenderObject scan_object(scan_mesh_view, m_material_instance_manager->pbr);
        // scan_object.set_material_data(PBRMaterialData::create(0.0f, 0.95f, 1.8f, glm::vec4(1.0f), 1.0f));

        // glm::mat4 mesh_matrix = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f)) * 
        //     m_network_scan->point_cloud().transform.get_model_matrix();

        // m_voxelizator->voxelize<PBRVertex>(
        //     blue_voxelize_prefab,
        //     scan_object.mesh_view(),
        //     mesh_matrix,
        //     &m_voxel_grid->local_voxel_write_list()
        // );

        m_path_planner.request_adjust_to_ground(
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
    if (std::isfinite(speed))
        m_car_speed = speed;
}

void Celeris::add_waypoint(glm::vec3 position) {
    m_waypoint_path.add_waypoint(position);
}

void Celeris::add_waypoint(const NonholonomicPos& position) {
    m_waypoint_path.add_waypoint(position);
}

void Celeris::delete_last_waypoint() {
    m_waypoint_path.delete_last_waypoint();
}

LidarScan* Celeris::network_scan() {
    return m_network_scan.get();
}

const Transform& Celeris::lidar_transform() const noexcept {
    return m_lidar_transform;
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

void Celeris::request_path_replan() {
    m_path_planner.request_path_replan(m_start_position, m_goal_position);
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
    return m_path_planner.request_result_snapshot();
}

void Celeris::display_path_planner_debug_controls() const {
    if (!ImGui::CollapsingHeader("Path planner")) {
        return;
    }

    PathPlanner::PathPlannerResult result = path_result_snapshot();

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
}

glm::vec3 Celeris::voxel_size() {
    return m_path_planner.request_voxel_size();
}

glm::vec3 Celeris::voxel_center_world_pos(const glm::ivec3& voxel_pos) {
    return m_path_planner.request_voxel_center_world_pos(voxel_pos);
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
    m_start_position.from_transform(rear_axle_transform_from_lidar_transform(m_lidar_transform));
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
