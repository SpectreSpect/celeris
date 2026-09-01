#include "new_celeris_visualizer.h"

#include "../voxel_grid_vulkan/voxel_grid.h"
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
            skybox_exposure) {
    add_child(m_gazelle_next);
}

void NewCelerisVisualizer::update() {
    LOG_METHOD();

    logger().check(m_celeris, "Celeris was null");

    update_gazelle_next_transform();
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