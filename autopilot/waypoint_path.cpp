#include "waypoint_path.h"

#include "../managers/material_instance_manager.h"
#include "../managers/mesh_manager.h"
#include "../renderer/lines/line_instance.h"
#include "../renderer/material_data_types.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>

namespace {
    constexpr glm::vec4 WAYPOINT_PATH_COLOR = glm::vec4(0.45f, 0.85f, 1.0f, 1.0f);
    constexpr float WAYPOINT_SPHERE_SCALE = 0.25f;
    constexpr float WAYPOINT_POSE_MARKER_SCALE = 0.45f;
    constexpr float WAYPOINT_VERTICAL_OFFSET = 0.5f;
}

WaypointPath::WaypointPath(
    VulkanEngine& engine,
    MeshManager& mesh_manager,
    MaterialInstanceManager& material_instance_manager,
    uint32_t max_line_count,
    float skybox_exposure)
    : m_max_line_count(max_line_count),
      m_skybox_exposure(skybox_exposure),
      m_mesh_manager(&mesh_manager),
      m_material_instance_manager(&material_instance_manager),
      m_sphere_mesh(&mesh_manager.sphere),
      m_sphere_material(&material_instance_manager.pbr),
      m_line_cloud(engine, mesh_manager.line_quad, material_instance_manager.line, max_line_count)
{
    m_line_cloud.set_material_data(LineMaterialData{
        .color = WAYPOINT_PATH_COLOR,
        .line_width_pixels = 5
    });

    add_child(m_line_cloud);
    refresh_visualization();
}

void WaypointPath::add_waypoint(glm::vec3 position) {
    m_waypoints.push_back(Waypoint{
        .position = glm::vec4(position, 1.0f),
        .theta = std::nullopt
    });
    refresh_visualization();
}

void WaypointPath::add_waypoint(const NonholonomicPos& position) {
    m_waypoints.push_back(Waypoint{
        .position = glm::vec4(position.pos, 1.0f),
        .theta = position.theta
    });
    refresh_visualization();
}

void WaypointPath::delete_last_waypoint() {
    if (m_waypoints.empty())
        return;

    m_waypoints.pop_back();
    refresh_visualization();
}

void WaypointPath::clear() {
    m_waypoints.clear();
    refresh_visualization();
}

const std::vector<WaypointPath::Waypoint>& WaypointPath::waypoints() const noexcept {
    return m_waypoints;
}

size_t WaypointPath::directional_waypoint_count() const noexcept {
    return static_cast<size_t>(std::count_if(
        m_waypoints.begin(),
        m_waypoints.end(),
        [](const Waypoint& waypoint) {
            return waypoint.directional();
        }
    ));
}

glm::vec3 WaypointPath::marker_vertical_offset() const noexcept {
    return glm::vec3(0.0f, WAYPOINT_VERTICAL_OFFSET, 0.0f);
}

std::vector<LineInstance> WaypointPath::make_lines() const {
    std::vector<LineInstance> lines;
    lines.reserve(std::min<size_t>(m_waypoints.size(), m_max_line_count));

    const glm::vec3 offset = marker_vertical_offset();
    for (uint32_t i = 1; i < m_waypoints.size() && lines.size() < m_max_line_count; i++) {
        lines.push_back(LineInstance{
            .p0 = m_waypoints[i - 1].world_position() + offset,
            .p1 = m_waypoints[i].world_position() + offset,
            .color = WAYPOINT_PATH_COLOR
        });
    }

    if (lines.empty()) {
        lines.push_back(LineInstance{
            .p0 = glm::vec3(0.0f),
            .p1 = glm::vec3(0.0f),
            .color = glm::vec4(0.0f)
        });
    }

    return lines;
}

void WaypointPath::refresh_visualization() {
    m_line_cloud.set_lines(make_lines());

    size_t sphere_id = 0;
    size_t pose_marker_id = 0;
    const glm::vec3 offset = marker_vertical_offset();

    for (const Waypoint& waypoint : m_waypoints) {
        if (waypoint.directional()) {
            while (m_pose_markers.size() <= pose_marker_id)
                create_pose_marker();

            SphericalPoseMarker& marker = *m_pose_markers[pose_marker_id];
            NonholonomicPos pose;
            pose.pos = waypoint.world_position();
            pose.theta = *waypoint.theta;
            set_marker_pose(marker, pose);
            marker.visible = true;

            pose_marker_id++;
            continue;
        }

        while (m_spheres.size() <= sphere_id)
            create_sphere();

        RenderObject& sphere = *m_spheres[sphere_id];
        sphere.transform.position = waypoint.world_position() + offset;
        sphere.visible = true;
        sphere_id++;
    }

    for (size_t i = sphere_id; i < m_spheres.size(); i++) {
        m_spheres[i]->visible = false;
    }

    for (size_t i = pose_marker_id; i < m_pose_markers.size(); i++) {
        m_pose_markers[i]->visible = false;
    }
}

void WaypointPath::set_marker_pose(SphericalPoseMarker& marker, const NonholonomicPos& position) {
    marker.transform.position = position.pos + marker_vertical_offset();
    marker.transform.rotation = glm::angleAxis(
        glm::pi<float>() - position.theta,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
}

void WaypointPath::create_sphere() {
    auto sphere = std::make_unique<RenderObject>(*m_sphere_mesh, *m_sphere_material);
    sphere->set_material_data(PBRMaterialData::create(
        1.0f,
        0.7f,
        m_skybox_exposure,
        WAYPOINT_PATH_COLOR
    ));
    sphere->transform.scale = glm::vec3(WAYPOINT_SPHERE_SCALE);
    add_child(*sphere);
    m_spheres.push_back(std::move(sphere));
}

void WaypointPath::create_pose_marker() {
    auto marker = std::make_unique<SphericalPoseMarker>(
        *m_mesh_manager,
        *m_material_instance_manager,
        PBRMaterialData::create(
            1.0f,
            0.7f,
            m_skybox_exposure,
            WAYPOINT_PATH_COLOR
        )
    );
    marker->transform.scale = glm::vec3(WAYPOINT_POSE_MARKER_SCALE);
    add_child(*marker);
    m_pose_markers.push_back(std::move(marker));
}
