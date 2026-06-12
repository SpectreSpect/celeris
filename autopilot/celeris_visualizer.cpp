#include "celeris_visualizer.h"
#include "../renderer/material_data_types.h"
#include "../a_star/a_star_structures.h"
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
                                     float skybox_exposure)
    :   m_celeris(&celeris),
        start_marker(mesh_manager, 
                     material_instance_manager, 
                     PBRMaterialData::create(1.0f, 0.7f, skybox_exposure, glm::vec4(1, 0, 0, 1))),
        goal_marker(mesh_manager, 
                    material_instance_manager, 
                    PBRMaterialData::create(1.0f, 0.7f, skybox_exposure, glm::vec4(0, 0, 1, 1))) {
    add_child(start_marker);
    add_child(goal_marker);
}

void CelerisVisualizer::set_start(const NonholonomicPos& nonholonomic_position) {
    set_marker_pose(start_marker, nonholonomic_position);
}

void CelerisVisualizer::set_goal(const NonholonomicPos& nonholonomic_position) {
    set_marker_pose(goal_marker, nonholonomic_position);
}

void CelerisVisualizer::set_start(const Transform& transform) {
    start_marker.transform = transform;
}

void CelerisVisualizer::set_goal(const Transform& transform) {
    goal_marker.transform = transform;
}

void CelerisVisualizer::update() {
    LOG_METHOD();

    logger.check(m_celeris, "Celeris was null");

    if (m_celeris->network_scan()) {
        set_start(m_celeris->network_scan()->point_cloud().transform);
        set_goal(m_celeris->goal_position());
    }
}

void CelerisVisualizer::set_marker_pose(SphericalPoseMarker& marker, NonholonomicPos nonholonomic_position) {
    marker.transform.position = nonholonomic_position.pos;
    marker.transform.rotation = glm::angleAxis(
        glm::pi<float>() - nonholonomic_position.theta,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
}