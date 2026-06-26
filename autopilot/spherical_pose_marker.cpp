#include "spherical_pose_marker.h"

#include "../managers/mesh_manager.h"
#include "../managers/material_instance_manager.h"

SphericalPoseMarker::SphericalPoseMarker(MeshManager& mesh_manager, 
                               MaterialInstanceManager& material_instance_manager,
                               PBRMaterialData material_data)
    :   SceneObject(),
        m_main_sphere(mesh_manager.sphere, material_instance_manager.pbr),
        m_direction_sphere(mesh_manager.sphere, material_instance_manager.pbr) {
    m_main_sphere.set_material_data(material_data);
    m_direction_sphere.set_material_data(material_data);

    m_direction_sphere.transform.scale = glm::vec3(0.3f);
    m_direction_sphere.transform.position = glm::vec3(-0.85f, 0, 0);

    add_child(m_main_sphere);
    add_child(m_direction_sphere);
}