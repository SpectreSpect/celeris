#pragma once

#include "../vulkan_self/logger/logger_header.h"
#include "../renderer/scene_object.h"
#include "spherical_pose_marker.h"
#include "gazelle_next.h"

class VehicleGeometry;
class NonholonomicPos;
class MeshManager;
class NewCeleris;

class NewCelerisVisualizer : public SceneObject {
public:
    _XCLASS_NAME(NewCelerisVisualizer);

    NewCelerisVisualizer(
        MeshManager& mesh_manager,
        MaterialInstanceManager& material_instance_manager, 
        NewCeleris& celeris, 
        const VehicleGeometry& vehicle_geometry,
        float skybox_exposure = 1.8f
    );

    void update();

    void gazelle_next_visible(bool visible);
    void voxel_grid_visible(bool visible);

private:
    NewCeleris* m_celeris = nullptr;

    GazelleNext m_gazelle_next;

    SphericalPoseMarker m_start_marker;
    SphericalPoseMarker m_goal_marker;

    void update_gazelle_next_transform();
    void set_marker_pose(
        SphericalPoseMarker& marker, 
        NonholonomicPos nonholonomic_position
    ); 
    void set_start(const NonholonomicPos& position);
    void set_goal(const NonholonomicPos& position);
};