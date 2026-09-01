#include "new_celeris_visualizer.h"

#include "../renderer/material_data_types.h"
#include "../voxel_grid_vulkan/voxel_grid.h"
#include "../a_star/a_star_structures.h"
#include "new_celeris.h"

NewCelerisVisualizer::NewCelerisVisualizer(
    MeshManager& mesh_manager,
    MaterialInstanceManager& material_instance_manager,
    NewCeleris& celeris, 
    const VehicleGeometry& vehicle_geometry,
    float skybox_exposure) 
    :   m_celeris(&celeris),
        m_gazelle_next(
            mesh_manager, 
            material_instance_manager,
            vehicle_geometry, 
            skybox_exposure),
        m_start_marker(
            mesh_manager,
            material_instance_manager,
            PBRMaterialData::create(1.0f, 0.7f, skybox_exposure, glm::vec4(1, 0, 0, 1))
        ),
        m_goal_marker(
            mesh_manager,
            material_instance_manager,
            PBRMaterialData::create(1.0f, 0.7f, skybox_exposure, glm::vec4(0, 0, 1, 1))
        ) {
    add_child(m_gazelle_next);
    add_child(m_start_marker);
    add_child(m_goal_marker);
}

void NewCelerisVisualizer::update() {
    LOG_METHOD();

    logger().check(m_celeris, "Celeris was null");

    update_gazelle_next_transform();
    set_start(m_celeris->start_position());
    set_goal(m_celeris->goal_position());
}

void NewCelerisVisualizer::gazelle_next_visible(bool visible) {
    m_gazelle_next.visible = visible;
}

void NewCelerisVisualizer::voxel_grid_visible(bool visible) {
    LOG_METHOD();

    logger().check(m_celeris->voxel_grid(), "Voxel grid was null");

    m_celeris->voxel_grid()->visible = visible;
}

void NewCelerisVisualizer::update_gazelle_next_transform() {
    Transform* lidar_transform = m_celeris->lidar_tranform();
    if (lidar_transform)
        m_gazelle_next.set_lidar_transform(*lidar_transform);
}

void NewCelerisVisualizer::set_marker_pose(
    SphericalPoseMarker& marker, 
    NonholonomicPos nonholonomic_position) 
{
    logger().check(m_celeris, "Celeris was null");

    glm::vec3 voxel_size = m_celeris->voxel_grid()->voxel_size();
    glm::vec3 vertical_offset = glm::vec3(0.0f, 0.5f * voxel_size.y, 0.0f);

    marker.transform.position = nonholonomic_position.pos + vertical_offset;
    marker.transform.rotation = glm::angleAxis(
        glm::pi<float>() - nonholonomic_position.theta,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
}

void NewCelerisVisualizer::set_start(const NonholonomicPos& position) {
    set_marker_pose(m_start_marker, position);
}

void NewCelerisVisualizer::set_goal(const NonholonomicPos& position) {
    set_marker_pose(m_goal_marker, position);
}