#pragma once

#include "spherical_pose_marker.h"
#include "../renderer/scene_object.h"
#include "../renderer/lines/line_cloud.h"

#include "../vulkan_self/logger/logger_header.h"

class Celeris;
class NonholonomicPos;

class CelerisVisualizer : public SceneObject {
public:
    _XCLASS_NAME(CelerisVisualizer);

    CelerisVisualizer(MeshManager& mesh_manager, 
                      MaterialInstanceManager& material_instance_manager, 
                      Celeris& celeris,
                      uint32_t max_path_line_count = 20000,
                      float skybox_exposure = 1.8f);
   
    void set_start(const NonholonomicPos& nonholonomic_position);
    void set_goal(const NonholonomicPos& nonholonomic_position);
    
    void set_start(const Transform& transform);
    void set_goal(const Transform& transform);

    void update();
private:
    uint32_t max_path_line_count = 0;
    uint32_t scan_generation = 0;
    
    Celeris* m_celeris = nullptr;

    SphericalPoseMarker m_start_marker;
    SphericalPoseMarker m_goal_marker;

    LineCloud m_path_line_cloud;

    void set_marker_pose(SphericalPoseMarker& marker, NonholonomicPos nonholonomic_position);
    std::vector<LineInstance> make_path_lines(const std::vector<NonholonomicPos>& path);
};