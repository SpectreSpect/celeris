#include "new_celeris_visualizer.h"

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

    Transform* lidar_transform = m_celeris->lidar_tranform();
    if (lidar_transform)
        m_gazelle_next.set_lidar_transform(*lidar_transform);
}