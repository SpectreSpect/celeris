#include "celeris_visualizer.h"
#include "../renderer/material_data_types.h"
#include "../a_star/a_star_structures.h"
#include "../managers/mesh_manager.h"
#include "../managers/material_instance_manager.h"
#include "../vulkan_self/utils.h"
#include "celeris.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <limits>

CelerisVisualizer::CelerisVisualizer(MeshManager& mesh_manager,
                                     MaterialInstanceManager& material_instance_manager,
                                     celeris::Celeris& celeris,
                                     const VehicleGeometry& vehicle_geometry,
                                     uint32_t max_path_line_count,
                                     float skybox_exposure)
    :   max_path_line_count(max_path_line_count),
        scan_generation(celeris.received_scan_count()),
        m_celeris(&celeris),
        m_start_marker(
            mesh_manager,
            material_instance_manager,
            PBRMaterialData::create(1.0f, 0.7f, skybox_exposure, glm::vec4(1, 0, 0, 1))
        ),
        m_goal_marker(
            mesh_manager,
            material_instance_manager,
            PBRMaterialData::create(1.0f, 0.7f, skybox_exposure, glm::vec4(0, 0, 1, 1))
        ),
        m_vehicle_marker(
            mesh_manager,
            material_instance_manager,
            PBRMaterialData::create(1.0f, 0.7f, skybox_exposure, glm::vec4(0, 1, 0, 1))
        ),
        m_lookahead_marker(
            mesh_manager,
            material_instance_manager,
            PBRMaterialData::create(1.0f, 0.7f, skybox_exposure, glm::vec4(1.0f, 0.0f, 0.9f, 1.0f))
        ),
        m_gazelle_next(mesh_manager, material_instance_manager, vehicle_geometry, skybox_exposure),
        linear_acceleration_arrow(
            *m_celeris->engine(),
            mesh_manager,
            material_instance_manager
        ),
        m_path_line_cloud(*m_celeris->engine(),
                   mesh_manager.line_quad,
                   material_instance_manager.line,
                   max_path_line_count),
        m_guide_path_line_cloud(*m_celeris->engine(),
                   mesh_manager.line_quad,
                   material_instance_manager.line,
                   max_path_line_count),
        m_explored_paths_line_cloud(*m_celeris->engine(),
                   mesh_manager.line_quad,
                   material_instance_manager.line,
                   max_path_line_count),
        m_unimpended_path_line_cloud(*m_celeris->engine(),
                   mesh_manager.line_quad,
                   material_instance_manager.line,
                   max_path_line_count),
        m_local_candidate_line_cloud(*m_celeris->engine(),
                   mesh_manager.line_quad,
                   material_instance_manager.line,
                   max_path_line_count),
        m_segment_switch_sphere_line_cloud(*m_celeris->engine(),
                   mesh_manager.line_quad,
                   material_instance_manager.line,
                   96),
        m_segment_switch_rear_axle_point(mesh_manager.sphere, material_instance_manager.pbr),
        m_point_map_point_cloud(
            *m_celeris->engine(),
            mesh_manager,
            material_instance_manager,
            m_celeris->voxel_point_map().map_point_buffer,
            celeris.voxel_point_map().m_map_point_count
        )
{
    m_path_line_cloud.set_material_data(LineMaterialData{
        .color = glm::vec4(1, 1, 1, 1),
        .line_width_pixels = 5
    });

    m_guide_path_line_cloud.set_material_data(LineMaterialData{
        .color = glm::vec4(0.3f, 1.0f, 0.3f, 1.0f),
        .line_width_pixels = 5
    });

    // PointCloud voxel_point_map(
    //     manager_bundle,
    //     celeris.voxel_point_map().map_point_buffer,
    //     celeris.voxel_point_map().m_map_point_count
    // );

    m_explored_paths_line_cloud.set_material_data(LineMaterialData{
        .color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
        .line_width_pixels = 3
    });

    m_unimpended_path_line_cloud.set_material_data(LineMaterialData{
        .color = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
        .line_width_pixels = 5
    });

    m_local_candidate_line_cloud.set_material_data(LineMaterialData{
        .color = glm::vec4(1.0f),
        .line_width_pixels = 4
    });

    m_segment_switch_sphere_line_cloud.set_material_data(LineMaterialData{
        .color = glm::vec4(1.0f, 0.85f, 0.05f, 1.0f),
        .line_width_pixels = 3
    });

    m_segment_switch_rear_axle_point.set_material_data(PBRMaterialData::create(
        1.0f,
        0.7f,
        skybox_exposure,
        glm::vec4(0.0f, 1.0f, 0.25f, 1.0f)
    ));
    m_segment_switch_rear_axle_point.transform.scale = glm::vec3(0.35f);

    // m_point_map_point_cloud.set_material_data(PointCloudMaterialData{
    //     .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
    // });

    add_child(m_start_marker);
    add_child(m_gazelle_next);
    add_child(m_goal_marker);
    add_child(m_vehicle_marker);
    add_child(m_lookahead_marker);
    add_child(m_path_line_cloud);
    add_child(m_guide_path_line_cloud);
    add_child(m_explored_paths_line_cloud);
    add_child(m_unimpended_path_line_cloud);
    add_child(m_local_candidate_line_cloud);
    add_child(m_segment_switch_sphere_line_cloud);
    add_child(m_segment_switch_rear_axle_point);
    add_child(m_celeris->waypoint_path());
    add_child(m_point_map_point_cloud);

    if (m_celeris->has_start_position())
        set_start(m_celeris->start_position());
    set_gazelle_mid_rear_axes_transform(m_celeris->vehicle_transform());
    if (m_celeris->has_goal_position())
        set_goal(m_celeris->goal_position());

    m_start_marker.visible = show_start_marker && m_celeris->has_start_position();
    m_goal_marker.visible = show_goal_marker && m_celeris->has_goal_position();
    m_lookahead_marker.visible = false;
    m_point_map_point_cloud.visible = show_voxel_point_map;
    m_segment_switch_sphere_line_cloud.visible = show_segment_switch_debug;
    m_segment_switch_rear_axle_point.visible = show_segment_switch_debug;
}

void CelerisVisualizer::set_start(const NonholonomicPos& nonholonomic_position) {
    set_marker_pose(m_start_marker, nonholonomic_position);
    set_gazelle_mid_rear_axes_transform(m_celeris->vehicle_transform());
    reset_marker_interpolation(m_start_marker, m_start_marker_interpolation);
}

void CelerisVisualizer::set_start(const Transform& transform) {
    m_start_marker.transform = transform;
    m_start_marker.transform.position += marker_vertical_offset();
    set_gazelle_mid_rear_axes_transform(m_celeris->vehicle_transform());
    reset_marker_interpolation(m_start_marker, m_start_marker_interpolation);
}

void CelerisVisualizer::set_goal(const NonholonomicPos& nonholonomic_position) {
    set_marker_pose(m_goal_marker, nonholonomic_position);
    reset_marker_interpolation(m_goal_marker, m_goal_marker_interpolation);
}

void CelerisVisualizer::set_goal(const Transform& transform) {
    m_goal_marker.transform = transform;
    m_goal_marker.transform.position += marker_vertical_offset();
    reset_marker_interpolation(m_goal_marker, m_goal_marker_interpolation);
}

void CelerisVisualizer::set_vehicle(const Transform& transform) {
    m_vehicle_marker.transform = transform;
    m_vehicle_marker.transform.position += marker_vertical_offset();
    reset_marker_interpolation(m_vehicle_marker, m_vehicle_marker_interpolation);
}

void CelerisVisualizer::set_vehicle(const NonholonomicPos& nonholonomic_position) {
    set_marker_pose(m_vehicle_marker, nonholonomic_position);
    reset_marker_interpolation(m_vehicle_marker, m_vehicle_marker_interpolation);
}

void CelerisVisualizer::set_car_pose_override(const NonholonomicPos& nonholonomic_position) {
    m_has_car_pose_override = true;
    m_car_pose_override = nonholonomic_position;
}

void CelerisVisualizer::clear_car_pose_override() {
    m_has_car_pose_override = false;
}

glm::vec3 CelerisVisualizer::get_start_marker_pos() {
    return m_start_marker.transform.position;
}

void CelerisVisualizer::update() {
    LOG_METHOD();

    logger().check(m_celeris, "Celeris was null");

    const auto now = std::chrono::steady_clock::now();
    const uint32_t received_scan_count = m_celeris->received_scan_count();
    const bool received_new_scan = received_scan_count != scan_generation;
    scan_generation = received_scan_count;

    if (m_celeris->has_start_position())
        set_start(m_celeris->start_position());
    if (m_celeris->has_goal_position())
        set_goal(m_celeris->goal_position());

    if (show_voxel_point_map) {
        m_point_map_point_cloud.set_instance_view(
            m_celeris->voxel_point_map().get_map_instance_view()
        );
    }

    if (m_has_car_pose_override)
        set_gazelle_pose(m_car_pose_override);
    else {
        set_gazelle_mid_rear_axes_transform(m_celeris->vehicle_transform());
    }
    

    m_start_marker.visible = show_start_marker && m_celeris->has_start_position();
    m_goal_marker.visible = show_goal_marker && m_celeris->has_goal_position();
    m_gazelle_next.visible = show_gazelle_next;
    m_point_map_point_cloud.visible = show_voxel_point_map;

    // interpolate_marker_pose(
    //     m_start_marker,
    //     m_start_marker_interpolation,
    //     m_celeris->start_position(),
    //     now,
    //     received_new_scan
    // );
    // interpolate_marker_pose(
    //     m_goal_marker,
    //     m_goal_marker_interpolation,
    //     m_celeris->goal_position(),
    //     now
    // );

    static const std::vector<LineInstance> hidden_lines{
        LineInstance{
            .p0 = glm::vec3(0.0f),
            .p1 = glm::vec3(0.0f),
            .color = glm::vec4(0.0f)
        }
    };

    PathPlanner::PathPlannerResult path_result = m_celeris->path_result_snapshot();
    glm::vec3 segment_switch_center{0.0f};
    const float segment_switch_radius =
        std::max(0.0f, m_celeris->local_planner_segment_switch_radius());
    const bool has_segment_switch_center =
        sample_path_at_s(
            path_result.nonholonomic_astar_path,
            m_celeris->local_planner_path_window_max_s(),
            segment_switch_center
        );
    if (show_segment_switch_debug && has_segment_switch_center) {
        const glm::vec3 visual_switch_center =
            segment_switch_center + marker_vertical_offset();
        m_segment_switch_sphere_line_cloud.set_lines(
            make_segment_switch_sphere_lines(
                visual_switch_center,
                segment_switch_radius
            )
        );
        m_segment_switch_rear_axle_point.transform.position = m_celeris->vehicle_transform().position;
    } else {
        m_segment_switch_sphere_line_cloud.set_lines(hidden_lines);
    }
    m_segment_switch_sphere_line_cloud.visible =
        show_segment_switch_debug && has_segment_switch_center;
    m_segment_switch_rear_axle_point.visible =
        show_segment_switch_debug && has_segment_switch_center;

    m_guide_path_line_cloud.set_lines(
        show_guide_path ? make_path_lines(path_result.plain_astar_path.path) : hidden_lines
    );
    m_path_line_cloud.set_lines(
        show_path
            ? make_path_lines(
                path_result.nonholonomic_astar_path,
                true,
                show_local_s_window,
                m_celeris->local_planner_path_window_min_s(),
                m_celeris->local_planner_path_window_max_s()
            )
            : hidden_lines
    );
    m_explored_paths_line_cloud.set_lines(
        show_explored_paths && !path_result.explored_paths.empty()
            ? path_result.explored_paths
            : hidden_lines
    );
    m_unimpended_path_line_cloud.set_lines(
        show_unimpeded_path ? make_path_lines(path_result.unimpended_path) : hidden_lines
    );

    interpolate_marker_pose(
        m_vehicle_marker,
        m_vehicle_marker_interpolation,
        NonholonomicPos::from_transform(m_celeris->vehicle_transform()),
        now,
        received_new_scan
    );
    m_vehicle_marker.visible = show_vehicle_marker;

    // const bool has_lookahead_point = m_celeris->has_local_planner_lookahead_point();
    // m_lookahead_marker.visible = show_lookahead_point && has_lookahead_point;
    // if (has_lookahead_point) {
    //     glm::vec3 lookahead_position = m_celeris->local_planner_lookahead_point();
    //     float best_dist2 = std::numeric_limits<float>::infinity();
    //     for (const NonholonomicPos& path_point : path_result.nonholonomic_astar_path) {
    //         const glm::vec2 diff{
    //             path_point.pos.x - lookahead_position.x,
    //             path_point.pos.z - lookahead_position.z
    //         };
    //         const float dist2 = glm::dot(diff, diff);
    //         if (dist2 < best_dist2) {
    //             best_dist2 = dist2;
    //             lookahead_position.y = path_point.pos.y;
    //         }
    //     }

    //     m_lookahead_marker.transform.position =
    //         lookahead_position +
    //         glm::vec3(0.0f, 0.2f + 0.8f * voxel_size().y, 0.0f);
    //     m_lookahead_marker.transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    //     m_lookahead_marker.transform.scale = glm::vec3(0.45f);
    // }

    m_local_candidate_line_cloud.set_lines(
        show_local_candidates
            ? make_local_candidate_lines(
                m_celeris->local_planner_candidates(),
                m_celeris->vehicle_lidar_transform().position.y
            )
            : hidden_lines
    );
}

void CelerisVisualizer::display_debug_controls() {
    if (ImGui::CollapsingHeader("Celeris path visualization")) {
        ImGui::Checkbox("Nonholonomic path", &show_path);
        ImGui::Checkbox("Plain A* path", &show_guide_path);
        ImGui::Checkbox("Explored paths", &show_explored_paths);
        ImGui::Checkbox("Unimpeded path", &show_unimpeded_path);
        ImGui::Checkbox("Waypoints", &m_celeris->waypoint_path().visible);
        ImGui::Checkbox("Start pose marker", &show_start_marker);
        ImGui::Checkbox("Goal pose marker", &show_goal_marker);
        ImGui::Checkbox("Vehicle pose marker", &show_vehicle_marker);
        ImGui::Checkbox("Pure Pursuit lookahead point", &show_lookahead_point);
        ImGui::Checkbox("Gazelle Next", &show_gazelle_next);
        ImGui::Checkbox("Local candidates", &show_local_candidates);
        ImGui::Checkbox("Local s window", &show_local_s_window);
        ImGui::Checkbox("Segment switch sphere", &show_segment_switch_debug);
        ImGui::Checkbox("Show voxel point map##visualizer_voxel_point_map", &show_voxel_point_map);
    }
}

glm::vec3 CelerisVisualizer::voxel_size() noexcept {
    return m_celeris->voxel_size();
}

glm::vec3 CelerisVisualizer::marker_vertical_offset() noexcept {
    return glm::vec3(0.0f, 0.5f * voxel_size().y, 0.0f);
}

glm::mat4 CelerisVisualizer::corrected_zero_lidar_transform() {
    return glm::translate(m_gazelle_next.zero_lidar_transform(), glm::vec3(0, voxel_size().y / 2.0f, 0));
}

void CelerisVisualizer::set_marker_pose(SphericalPoseMarker& marker, NonholonomicPos nonholonomic_position) {
    marker.transform.position = nonholonomic_position.pos + marker_vertical_offset();
    marker.transform.rotation = glm::angleAxis(
        glm::pi<float>() - nonholonomic_position.theta,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
}

void CelerisVisualizer::set_gazelle_pose(const NonholonomicPos& nonholonomic_position) {
    Transform rear_axle_transform;
    rear_axle_transform.position = nonholonomic_position.pos + marker_vertical_offset();
    rear_axle_transform.rotation = glm::angleAxis(
        -nonholonomic_position.theta,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    m_gazelle_next.set_rear_axle_transform(rear_axle_transform);
}

void CelerisVisualizer::set_gazelle_lidar_transform(const Transform& lidar_transform) {
    Transform visual_lidar_transform = lidar_transform;
    visual_lidar_transform.position += marker_vertical_offset();
    m_gazelle_next.set_lidar_transform(visual_lidar_transform);
    // m_gazelle_next.transform = visual_lidar_transform;
}

void CelerisVisualizer::set_gazelle_mid_rear_axes_transform(const Transform& mid_rear_axes_transform) {
    glm::mat4 vehicle_transform = m_celeris->vehicle_transform().get_model_matrix();
    glm::mat4 lidar_transform = vehicle_transform * m_gazelle_next.mid_rear_axes_to_lidar_transform();
    glm::mat4 zero_lidar_transform = lidar_transform * corrected_zero_lidar_transform();
    m_gazelle_next.transform = Transform::from_matrix(zero_lidar_transform);
}

void CelerisVisualizer::reset_marker_interpolation(
    SphericalPoseMarker& marker,
    MarkerInterpolationState& state)
{
    state.previous_position = marker.transform.position;
    state.target_position = marker.transform.position;
    state.previous_rotation = marker.transform.rotation;
    state.target_rotation = marker.transform.rotation;
    state.sample_time = std::chrono::steady_clock::now();
    state.initialized = true;
}

void CelerisVisualizer::interpolate_marker_pose(
    SphericalPoseMarker& marker,
    MarkerInterpolationState& state,
    const NonholonomicPos& target,
    std::chrono::steady_clock::time_point now,
    bool force_new_sample)
{
    const glm::vec3 target_position = target.pos + marker_vertical_offset();
    const glm::quat target_rotation = glm::normalize(glm::angleAxis(
        glm::pi<float>() - target.theta,
        glm::vec3(0.0f, 1.0f, 0.0f)
    ));

    if (!state.initialized) {
        set_marker_pose(marker, target);
        reset_marker_interpolation(marker, state);
        state.sample_time = now;
        return;
    }

    const glm::vec3 position_delta = target_position - state.target_position;
    const bool position_changed = glm::dot(position_delta, position_delta) > 1e-8f;
    const bool rotation_changed =
        std::abs(glm::dot(target_rotation, state.target_rotation)) < 0.999999f;

    if (force_new_sample || position_changed || rotation_changed) {
        state.sample_interval = std::clamp(
            std::chrono::duration<float>(now - state.sample_time).count(),
            1.0f / 120.0f,
            0.5f
        );
        state.previous_position = marker.transform.position;
        state.previous_rotation = marker.transform.rotation;
        state.target_position = target_position;
        state.target_rotation = target_rotation;
        state.sample_time = now;
    }

    const float interpolation_time =
        std::chrono::duration<float>(now - state.sample_time).count();
    const float blend = std::clamp(
        interpolation_time / state.sample_interval,
        0.0f,
        1.0f
    );

    marker.transform.position = glm::mix(
        state.previous_position,
        state.target_position,
        blend
    );
    marker.transform.rotation = glm::normalize(glm::slerp(
        state.previous_rotation,
        state.target_rotation,
        blend
    ));
}

std::vector<LineInstance> CelerisVisualizer::make_path_lines(
    const std::vector<NonholonomicPos>& path,
    bool override_color,
    bool highlight_s_window,
    float window_min_s,
    float window_max_s)
{
    std::vector<LineInstance> path_lines;
    path_lines.reserve(std::min<size_t>(path.size() * 3, max_path_line_count));

    auto append_line = [&](glm::vec3 p0, glm::vec3 p1, glm::vec4 color) {
        if (path_lines.size() >= max_path_line_count)
            return;
        path_lines.push_back(LineInstance{
            .p0 = p0 + glm::vec3(0, 0.2f, 0),
            .p1 = p1 + glm::vec3(0, 0.2f, 0),
            .color = color
        });
    };

    const bool has_valid_s_window =
        highlight_s_window &&
        std::isfinite(window_min_s) &&
        std::isfinite(window_max_s) &&
        window_max_s > window_min_s + Utils::eps;

    float current_s = 0.0f;
    for (uint32_t i = 1; i < path.size() && path_lines.size() < max_path_line_count; i++) {
        glm::vec4 line_color = glm::vec4(1, 0, 0, 1);
        glm::vec4 active_line_color = glm::vec4(1.0f, 0.0f, 0.9f, 1.0f);
        if (path[i].dir == -1) {
            line_color = glm::vec4(0, 0, 1, 1);
            active_line_color = glm::vec4(0.0f, 0.95f, 1.0f, 1.0f);
        }

        if (!override_color) {
            append_line(path[i - 1].pos, path[i].pos, glm::vec4(1.0f));
            continue;
        }

        const glm::vec3 p0 = path[i - 1].pos;
        const glm::vec3 p1 = path[i].pos;
        const float segment_length = glm::distance(
            glm::vec2(p0.x, p0.z),
            glm::vec2(p1.x, p1.z)
        );
        const float next_s = current_s + segment_length;

        if (!has_valid_s_window || segment_length <= Utils::eps) {
            append_line(p0, p1, line_color);
            current_s = next_s;
            continue;
        }

        const bool segment_intersects_window =
            next_s >= window_min_s &&
            current_s <= window_max_s;
        if (!segment_intersects_window) {
            append_line(p0, p1, line_color * glm::vec4(0.45f, 0.45f, 0.45f, 1.0f));
            current_s = next_s;
            continue;
        }

        const float active_start_s = std::clamp(window_min_s, current_s, next_s);
        const float active_end_s = std::clamp(window_max_s, current_s, next_s);
        const float active_start_t = (active_start_s - current_s) / segment_length;
        const float active_end_t = (active_end_s - current_s) / segment_length;
        const glm::vec3 active_start = glm::mix(p0, p1, active_start_t);
        const glm::vec3 active_end = glm::mix(p0, p1, active_end_t);
        const glm::vec4 inactive_color = line_color * glm::vec4(0.45f, 0.45f, 0.45f, 1.0f);

        if (active_start_t > Utils::eps)
            append_line(p0, active_start, inactive_color);

        if (active_end_t > active_start_t + Utils::eps)
            append_line(active_start, active_end, active_line_color);

        if (active_end_t < 1.0f - Utils::eps)
            append_line(active_end, p1, inactive_color);

        current_s = next_s;
    }

    if (path_lines.empty())
        path_lines.push_back(LineInstance{.p0 = glm::vec3(0.0f),
                                            .p1 = glm::vec3(0.0f),
                                            .color = glm::vec4(0.0f)});

    return path_lines;
}

std::vector<LineInstance> CelerisVisualizer::make_path_lines(const std::vector<glm::vec3>& path) {
    std::vector<LineInstance> path_lines;
    path_lines.reserve(std::min<size_t>(path.size(), max_path_line_count));

    for (uint32_t i = 1; i < path.size() && path_lines.size() < max_path_line_count; i++) {
        path_lines.push_back(LineInstance{
            .p0 = path[i - 1] + glm::vec3(0, 0.2f, 0),
            .p1 = path[i] + glm::vec3(0, 0.2f, 0),
            .color = glm::vec4(1, 0, 0, 1)
        });
    }

    if (path_lines.empty())
        path_lines.push_back(LineInstance{.p0 = glm::vec3(0.0f),
                                            .p1 = glm::vec3(0.0f),
                                            .color = glm::vec4(0.0f)});

    return path_lines;
}

std::vector<LineInstance> CelerisVisualizer::make_path_lines(const std::vector<glm::ivec3>& path) {
    std::vector<LineInstance> path_lines;
    path_lines.reserve(std::min<size_t>(path.size(), max_path_line_count));

    for (uint32_t i = 1; i < path.size() && path_lines.size() < max_path_line_count; i++) {
        glm::vec3 p0 = m_celeris->voxel_center_world_pos(path[i - 1]);
        glm::vec3 p1 = m_celeris->voxel_center_world_pos(path[i]);
        p0.y -= 0.5f * voxel_size().y;
        p1.y -= 0.5f * voxel_size().y;

        path_lines.push_back(LineInstance{
            .p0 = p0 + glm::vec3(0, 0.2f, 0),
            .p1 = p1 + glm::vec3(0, 0.2f, 0),
            .color = glm::vec4(1, 1, 1, 1)
        });
    }

    if (path_lines.empty())
        path_lines.push_back(LineInstance{.p0 = glm::vec3(0.0f),
                                            .p1 = glm::vec3(0.0f),
                                            .color = glm::vec4(0.0f)});

    return path_lines;
}

std::vector<LineInstance> CelerisVisualizer::make_local_candidate_lines(
    const std::vector<Vehicle::SimulationControlCandidate>& candidates,
    float height)
{
    std::vector<LineInstance> candidate_lines;
    candidate_lines.reserve(max_path_line_count);

    const glm::vec3 base_offset(0.0f, 0.35f, 0.0f);
    const glm::vec3 selected_offset(0.0f, 0.7f, 0.0f);
    const glm::vec3 good_color(0.0f, 0.95f, 0.18f);
    const glm::vec3 bad_color(1.0f, 0.08f, 0.02f);
    constexpr glm::vec4 selected_color = glm::vec4(0.0f, 0.95f, 1.0f, 1.0f);

    float min_loss = std::numeric_limits<float>::infinity();
    float max_loss = -std::numeric_limits<float>::infinity();
    for (const Vehicle::SimulationControlCandidate& candidate : candidates) {
        if (!std::isfinite(candidate.loss))
            continue;

        min_loss = std::min(min_loss, candidate.loss);
        max_loss = std::max(max_loss, candidate.loss);
    }

    auto candidate_color = [&](size_t candidate_id) -> glm::vec4 {
        const float rank_t = candidates.size() > 1
            ? static_cast<float>(candidate_id) / static_cast<float>(candidates.size() - 1)
            : 0.0f;

        float loss_t = rank_t;
        if (std::isfinite(min_loss) && std::isfinite(max_loss) && max_loss > min_loss + 1e-5f) {
            loss_t = std::clamp(
                (candidates[candidate_id].loss - min_loss) / (max_loss - min_loss),
                0.0f,
                1.0f
            );
        }

        const glm::vec3 color = good_color * (1.0f - loss_t) + bad_color * loss_t;
        const float alpha = 0.78f - 0.34f * loss_t;
        return glm::vec4(color, alpha);
    };

    auto append_candidate = [&](size_t candidate_id, bool selected) {
        const std::vector<glm::vec2>& trajectory = candidates[candidate_id].trajectory;
        if (trajectory.size() < 2)
            return;

        const glm::vec4 color = selected ? selected_color : candidate_color(candidate_id);
        const glm::vec3 offset = selected
            ? selected_offset
            : base_offset + glm::vec3(0.0f, 0.0025f * static_cast<float>(candidate_id), 0.0f);

        for (size_t i = 1; i < trajectory.size() && candidate_lines.size() < max_path_line_count; i++) {
            candidate_lines.push_back(LineInstance{
                .p0 = glm::vec3(trajectory[i - 1].x, height, trajectory[i - 1].y) + offset,
                .p1 = glm::vec3(trajectory[i].x, height, trajectory[i].y) + offset,
                .color = color
            });
        }
    };

    for (size_t remaining = candidates.size(); remaining > 1 && candidate_lines.size() < max_path_line_count; remaining--) {
        append_candidate(remaining - 1, false);
    }
    if (!candidates.empty() && candidate_lines.size() < max_path_line_count)
        append_candidate(0, true);

    if (candidate_lines.empty())
        candidate_lines.push_back(LineInstance{.p0 = glm::vec3(0.0f),
                                               .p1 = glm::vec3(0.0f),
                                               .color = glm::vec4(0.0f)});

    return candidate_lines;
}

std::vector<LineInstance> CelerisVisualizer::make_segment_switch_sphere_lines(
    glm::vec3 center,
    float radius)
{
    constexpr size_t circle_segments = 32;
    constexpr glm::vec4 color = glm::vec4(1.0f, 0.85f, 0.05f, 1.0f);

    std::vector<LineInstance> lines;
    lines.reserve(circle_segments * 3u);

    if (!std::isfinite(radius) || radius <= Utils::eps) {
        lines.push_back(LineInstance{
            .p0 = center,
            .p1 = center,
            .color = glm::vec4(0.0f)
        });
        return lines;
    }

    auto append_circle = [&](glm::vec3 axis_a, glm::vec3 axis_b) {
        for (size_t i = 0; i < circle_segments; i++) {
            const float t0 =
                glm::two_pi<float>() * static_cast<float>(i) /
                static_cast<float>(circle_segments);
            const float t1 =
                glm::two_pi<float>() * static_cast<float>(i + 1u) /
                static_cast<float>(circle_segments);
            lines.push_back(LineInstance{
                .p0 = center + radius * (std::cos(t0) * axis_a + std::sin(t0) * axis_b),
                .p1 = center + radius * (std::cos(t1) * axis_a + std::sin(t1) * axis_b),
                .color = color
            });
        }
    };

    append_circle(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    append_circle(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    append_circle(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    return lines;
}

bool CelerisVisualizer::sample_path_at_s(
    const std::vector<NonholonomicPos>& path,
    float s,
    glm::vec3& output) const
{
    if (path.empty() || !std::isfinite(s))
        return false;

    if (path.size() == 1 || s <= 0.0f) {
        output = path.front().pos;
        return true;
    }

    float current_s = 0.0f;
    for (size_t i = 1; i < path.size(); i++) {
        const glm::vec3 p0 = path[i - 1].pos;
        const glm::vec3 p1 = path[i].pos;
        const float segment_length = glm::distance(
            glm::vec2(p0.x, p0.z),
            glm::vec2(p1.x, p1.z)
        );

        if (segment_length <= Utils::eps)
            continue;

        const float next_s = current_s + segment_length;
        if (s <= next_s || i + 1u == path.size()) {
            const float t = std::clamp((s - current_s) / segment_length, 0.0f, 1.0f);
            output = glm::mix(p0, p1, t);
            return true;
        }

        current_s = next_s;
    }

    output = path.back().pos;
    return true;
}
