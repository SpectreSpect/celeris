#include "celeris_visualizer.h"
#include "../renderer/material_data_types.h"
#include "../a_star/a_star_structures.h"
#include "../managers/mesh_manager.h"
#include "../managers/material_instance_manager.h"
#include "celeris.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

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
        m_path_line_cloud(*m_celeris->engine(),
                   mesh_manager.line_quad,
                   material_instance_manager.line,
                   max_path_line_count),
        m_guide_path_line_cloud(*m_celeris->engine(),
                   mesh_manager.line_quad,
                   material_instance_manager.line,
                   max_path_line_count) {
    m_path_line_cloud.set_material_data(LineMaterialData{
        .color = glm::vec4(1, 1, 1, 1),
        .line_width_pixels = 5
    });

    m_guide_path_line_cloud.set_material_data(LineMaterialData{
        // .color = glm::vec4(0.078, 0.663, 0.494, 1.0),
        .color = glm::vec4(0.3f, 1.0f, 0.3f, 1.0f),
        .line_width_pixels = 5
    });

    add_child(m_start_marker);
    add_child(m_goal_marker);
    add_child(m_path_line_cloud);
    add_child(m_guide_path_line_cloud);

    set_goal(m_celeris->goal_position());
}

void CelerisVisualizer::set_start(const NonholonomicPos& nonholonomic_position) {
    set_marker_pose(m_start_marker, nonholonomic_position);
}

void CelerisVisualizer::set_goal(const NonholonomicPos& nonholonomic_position) {
    set_marker_pose(m_goal_marker, nonholonomic_position);
}

void CelerisVisualizer::set_start(const Transform& transform) {
    m_start_marker.transform = transform;
}

void CelerisVisualizer::set_goal(const Transform& transform) {
    m_goal_marker.transform = transform;
}

void CelerisVisualizer::update() {
    LOG_METHOD();

    logger.check(m_celeris, "Celeris was null");

    set_start(m_celeris->start_position());
    set_goal(m_celeris->goal_position());
    m_path_line_cloud.set_lines(make_path_lines(m_celeris->planner().state_path));
    // if (m_celeris->planner().state_explored_paths.size() > 0)
    m_guide_path_line_cloud.set_lines(make_path_lines(m_celeris->planner().state_plain_astar_path.path));

    // if (scan_generation != m_celeris->received_scan_count()) {
    //     scan_generation = m_celeris->received_scan_count();

    //     if (m_celeris->network_scan()) {
    //         set_start(m_celeris->network_scan()->point_cloud().transform);
    //         set_goal(m_celeris->goal_position());
    //         m_path_line_cloud.set_lines(make_path_lines(m_celeris->planner().state_path));
    //     }
    // }
}

void CelerisVisualizer::set_marker_pose(SphericalPoseMarker& marker, NonholonomicPos nonholonomic_position) {
    marker.transform.position = nonholonomic_position.pos;
    marker.transform.rotation = glm::angleAxis(
        glm::pi<float>() - nonholonomic_position.theta,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
}

std::vector<LineInstance> CelerisVisualizer::make_path_lines(const std::vector<NonholonomicPos>& path) {
    std::vector<LineInstance> path_lines;
    path_lines.reserve(std::min<size_t>(path.size(), max_path_line_count));

    for (uint32_t i = 1; i < path.size() && path_lines.size() < max_path_line_count; i++) {
        glm::vec4 line_color = glm::vec4(1, 0, 0, 1);
        if (path[i].dir == -1)
            line_color = glm::vec4(0, 0, 1, 1);

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
        path_lines.push_back(LineInstance{
            .p0 = glm::vec3(path[i - 1]) + glm::vec3(0, 0.2f, 0),
            .p1 = glm::vec3(path[i]) + glm::vec3(0, 0.2f, 0),
            .color = glm::vec4(1, 1, 1, 1)
        });
    }

    if (path_lines.empty())
        path_lines.push_back(LineInstance{.p0 = glm::vec3(0.0f),
                                            .p1 = glm::vec3(0.0f),
                                            .color = glm::vec4(0.0f)});

    return path_lines;
}
