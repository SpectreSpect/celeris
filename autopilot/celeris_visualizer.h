#pragma once

#include "spherical_pose_marker.h"
#include "../renderer/scene_object.h"

#include "../vulkan_self/logger/logger_header.h"

class Celeris;
class NonholonomicPos;

class CelerisVisualizer : public SceneObject {
public:
    _XCLASS_NAME(CelerisVisualizer);

    CelerisVisualizer(MeshManager& mesh_manager, 
                      MaterialInstanceManager& material_instance_manager, 
                      Celeris& celeris,
                      float skybox_exposure = 1.8f);
   
    void set_start(const NonholonomicPos& nonholonomic_position);
    void set_goal(const NonholonomicPos& nonholonomic_position);
    
    void set_start(const Transform& transform);
    void set_goal(const Transform& transform);

    void update();
private:
    Celeris* m_celeris = nullptr;

    SphericalPoseMarker start_marker;
    SphericalPoseMarker goal_marker;

    void set_marker_pose(SphericalPoseMarker& marker, NonholonomicPos nonholonomic_position);
};