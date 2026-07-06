#include "celeris_visualizer.h"
#include "../renderer/material_data_types.h"
#include "../a_star/a_star_structures.h"
#include "../managers/mesh_manager.h"
#include "../managers/material_instance_manager.h"
#include "celeris.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>

// namespace {
//     void set_marker_pose(SphericalPoseMarker& marker, NonholonomicPos nonholonomic_position) {
//         marker.transform.position = nonholonomic_position.pos;
//         marker.transform.rotation = glm::angleAxis(
//             glm::pi<float>() - nonholonomic_position.theta,
//             glm::vec3(0.0f, 1.0f, 0.0f)
//         );
//     }
// }

CelerisVisualizer::CelerisVisualizer(MeshManager& mesh_manager, 
                                     MaterialInstanceManager& material_instance_manager, 
                                     Celeris& celeris,
                                     const VehicleGeometry& vehicle_geometry,
                                     uint32_t max_path_line_count,
                                     float skybox_exposure)
    :   max_path_line_count(max_path_line_count),
        scan_generation(celeris.received_scan_count()),
        m_celeris(&celeris),
        m_start_marker(mesh_manager, 
                     material_instance_manager, 
                     PBRMaterialData::create(1.0f, 0.7f, skybox_exposure, glm::vec4(1, 0, 0, 1))),
        m_goal_marker(mesh_manager, 
                    material_instance_manager, 
                    PBRMaterialData::create(1.0f, 0.7f, skybox_exposure, glm::vec4(0, 0, 1, 1))),
        m_gazelle_next(mesh_manager, material_instance_manager, vehicle_geometry, skybox_exposure),
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
        // VulkanEngine& engine, MeshManager& mesh_manager, MaterialInstanceManager& material_instance_manager, VulkanBuffer& instance_buffer, uint32_t instance_count
        m_point_map_point_cloud(
            *m_celeris->engine(), 
            mesh_manager, 
            material_instance_manager, 
            m_celeris->voxel_point_map().map_point_buffer, 
            celeris.voxel_point_map().m_map_point_count
        ) {
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

    // m_point_map_point_cloud.set_material_data(PointCloudMaterialData{
    //     .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
    // });

    add_child(m_start_marker);
    add_child(m_gazelle_next);
    add_child(m_goal_marker);
    add_child(m_path_line_cloud);
    add_child(m_guide_path_line_cloud);
    add_child(m_explored_paths_line_cloud);
    add_child(m_unimpended_path_line_cloud);
    add_child(m_point_map_point_cloud);


    set_start(m_celeris->start_position());
    set_goal(m_celeris->goal_position());
}

void CelerisVisualizer::set_start(const NonholonomicPos& nonholonomic_position) {
    set_marker_pose(m_start_marker, nonholonomic_position);
    set_gazelle_pose_from_lidar_transform();
    reset_marker_interpolation(m_start_marker, m_start_marker_interpolation);
}

void CelerisVisualizer::set_goal(const NonholonomicPos& nonholonomic_position) {
    set_marker_pose(m_goal_marker, nonholonomic_position);
    reset_marker_interpolation(m_goal_marker, m_goal_marker_interpolation);
}

void CelerisVisualizer::set_start(const Transform& transform) {
    m_start_marker.transform = transform;
    m_start_marker.transform.position += marker_vertical_offset();
    set_gazelle_pose_from_lidar_transform();
    reset_marker_interpolation(m_start_marker, m_start_marker_interpolation);
}

void CelerisVisualizer::set_goal(const Transform& transform) {
    m_goal_marker.transform = transform;
    m_goal_marker.transform.position += marker_vertical_offset();
    reset_marker_interpolation(m_goal_marker, m_goal_marker_interpolation);
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

    m_point_map_point_cloud.set_instance_view(
        m_celeris->voxel_point_map().get_map_instance_view()
    );

    set_start(m_celeris->start_position());
    set_goal(m_celeris->goal_position());
    if (m_has_car_pose_override)
        set_gazelle_pose(m_car_pose_override);
    else
        set_gazelle_pose_from_lidar_transform();

    m_start_marker.visible = show_start_marker;
    m_gazelle_next.visible = show_gazelle_next;

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
    m_guide_path_line_cloud.set_lines(
        show_guide_path ? make_path_lines(path_result.plain_astar_path.path) : hidden_lines
    );
    m_path_line_cloud.set_lines(
        show_path ? make_path_lines(path_result.nonholonomic_astar_path, true) : hidden_lines
    );
    m_explored_paths_line_cloud.set_lines(
        show_explored_paths && !path_result.explored_paths.empty()
            ? path_result.explored_paths
            : hidden_lines
    );
    m_unimpended_path_line_cloud.set_lines(
        show_unimpeded_path ? make_path_lines(path_result.unimpended_path) : hidden_lines
    );

}

void CelerisVisualizer::display_debug_controls() {
    if (ImGui::CollapsingHeader("Celeris path visualization")) {
        ImGui::Checkbox("Nonholonomic path", &show_path);
        ImGui::Checkbox("Plain A* path", &show_guide_path);
        ImGui::Checkbox("Explored paths", &show_explored_paths);
        ImGui::Checkbox("Unimpeded path", &show_unimpeded_path);
        ImGui::Checkbox("Start pose marker", &show_start_marker);
        ImGui::Checkbox("Gazelle Next", &show_gazelle_next);
    }
}

glm::vec3 CelerisVisualizer::voxel_size() noexcept {
    return m_celeris->voxel_size();
}

glm::vec3 CelerisVisualizer::marker_vertical_offset() noexcept {
    return glm::vec3(0.0f, 0.5f * voxel_size().y, 0.0f);
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
    rear_axle_transform.position = nonholonomic_position.pos;
    rear_axle_transform.rotation = glm::angleAxis(
        -nonholonomic_position.theta,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    m_gazelle_next.set_rear_axle_transform(rear_axle_transform);
}

void CelerisVisualizer::set_gazelle_pose_from_lidar_transform() {
    m_gazelle_next.set_lidar_transform(m_celeris->lidar_transform());
    m_gazelle_next.transform.position += marker_vertical_offset();
    // m_gazelle_next.transform = m_celeris->lidar_transform();
    // m_gazelle_next.transform.rotation = glm::normalize(
    //     m_gazelle_next.transform.rotation *
    //     glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f))
    // );
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

std::vector<LineInstance> CelerisVisualizer::make_path_lines(const std::vector<NonholonomicPos>& path, bool override_color) {
    std::vector<LineInstance> path_lines;
    path_lines.reserve(std::min<size_t>(path.size(), max_path_line_count));

    for (uint32_t i = 1; i < path.size() && path_lines.size() < max_path_line_count; i++) {
        glm::vec4 line_color = glm::vec4(1, 0, 0, 1);
        if (path[i].dir == -1)
            line_color = glm::vec4(0, 0, 1, 1);
        
        if (!override_color)
            line_color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

        path_lines.push_back(LineInstance{
            .p0 = path[i - 1].pos + glm::vec3(0, 0.2f, 0),
            .p1 = path[i].pos + glm::vec3(0, 0.2f, 0),
            .color = line_color
        });
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
