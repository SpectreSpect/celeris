#pragma once

#include "../vulkan_self/logger/logger_header.h"
#include "../renderer/lines/line_cloud.h"
#include "../renderer/scene_object.h"
#include "spherical_pose_marker.h"
#include "gazelle_next.h"

class VehicleGeometry;
class NonholonomicPos;
class VulkanEngine;
class MeshManager;
class NewCeleris;

class NewCelerisVisualizer : public SceneObject {
public:
    _XCLASS_NAME(NewCelerisVisualizer);

    NewCelerisVisualizer(
        VulkanEngine& engine,
        MeshManager& mesh_manager,
        MaterialInstanceManager& material_instance_manager, 
        NewCeleris& celeris, 
        const VehicleGeometry& vehicle_geometry,
        float skybox_exposure = 1.8f,
        uint32_t max_path_line_count = 20000
    );

    void update();

    void gazelle_next_visible(bool visible);
    void voxel_grid_visible(bool visible);
    void guide_path_visible(bool visible);

private:
    NewCeleris* m_celeris = nullptr;

    uint32_t m_max_path_line_count = 0;

    GazelleNext m_gazelle_next;

    SphericalPoseMarker m_start_marker;
    SphericalPoseMarker m_goal_marker;

    LineCloud m_path_line_cloud;
    
    LineCloud m_guide_path_line_cloud; // Done

    // LineCloud m_explored_paths_line_cloud;
    // LineCloud m_unimpended_path_line_cloud;


    void update_gazelle_next_transform();
    
    void set_marker_pose(
        SphericalPoseMarker& marker, 
        NonholonomicPos nonholonomic_position
    ); 
    void set_start(const NonholonomicPos& position);
    void set_goal(const NonholonomicPos& position);

    std::vector<LineInstance> get_line_instances(
        const std::vector<NonholonomicPos> path, 
        float y_offset = 0.2f
    );
    std::vector<LineInstance> get_line_instances(
        const std::vector<glm::ivec3> path, 
        float y_offset = 0.2f
    );
};