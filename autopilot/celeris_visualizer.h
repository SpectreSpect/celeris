#pragma once

#include "celeris.h"
#include "gazelle_next.h"
#include "spherical_pose_marker.h"
#include "../a_star/a_star_structures.h"
#include "../renderer/scene_object.h"
#include "../renderer/lines/line_cloud.h"
#include "../renderer/point_cloud/point_cloud.h"
#include "../renderer/render_object.h"

#include "../vulkan_self/logger/logger_header.h"

#include <glm/vec3.hpp>
#include <glm/ext/vector_int3.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>
#include <chrono>
#include <memory>

struct VehicleGeometry;
class Mesh;
class SlotPassInstance;

class CelerisVisualizer : public SceneObject {
public:
    _XCLASS_NAME(CelerisVisualizer);

    CelerisVisualizer(MeshManager& mesh_manager, 
                      MaterialInstanceManager& material_instance_manager, 
                      Celeris& celeris,
                      const VehicleGeometry& vehicle_geometry,
                      uint32_t max_path_line_count = 20000,
                      float skybox_exposure = 1.8f);
   
    void set_start(const NonholonomicPos& nonholonomic_position);
    void set_goal(const NonholonomicPos& nonholonomic_position);
    
    void set_start(const Transform& transform);
    void set_goal(const Transform& transform);
    void set_car_pose_override(const NonholonomicPos& nonholonomic_position);
    void clear_car_pose_override();

    glm::vec3 get_start_marker_pos();

    void update();
    void display_debug_controls();

private:
    struct MarkerInterpolationState {
        glm::vec3 previous_position{0.0f};
        glm::vec3 target_position{0.0f};
        glm::quat previous_rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::quat target_rotation{1.0f, 0.0f, 0.0f, 0.0f};
        std::chrono::steady_clock::time_point sample_time;
        float sample_interval = 1.0f / 30.0f;
        bool initialized = false;
    };

    uint32_t max_path_line_count = 0;
    uint32_t scan_generation = 0;
    
    Celeris* m_celeris = nullptr;
    Mesh* m_waypoint_sphere_mesh = nullptr;
    SlotPassInstance* m_waypoint_sphere_material = nullptr;

    SphericalPoseMarker m_start_marker;
    SphericalPoseMarker m_goal_marker;
    GazelleNext m_gazelle_next;

    LineCloud m_path_line_cloud;
    LineCloud m_guide_path_line_cloud;
    LineCloud m_explored_paths_line_cloud;
    LineCloud m_unimpended_path_line_cloud;
    LineCloud m_waypoint_path_line_cloud;

    PointCloud m_point_map_point_cloud;
    std::vector<std::unique_ptr<RenderObject>> m_waypoint_spheres;

    MarkerInterpolationState m_start_marker_interpolation;
    MarkerInterpolationState m_goal_marker_interpolation;

    bool show_path = true;
    bool show_guide_path = true;
    bool show_explored_paths = false;
    bool show_unimpeded_path = true;
    bool show_waypoints = true;
    bool show_start_marker = false;
    bool show_gazelle_next = true;
    bool m_has_car_pose_override = false;
    NonholonomicPos m_car_pose_override;

private:
    glm::vec3 voxel_size() noexcept;
    glm::vec3 marker_vertical_offset() noexcept;

    void set_marker_pose(SphericalPoseMarker& marker, NonholonomicPos nonholonomic_position);
    void set_gazelle_pose(const NonholonomicPos& nonholonomic_position);
    void set_gazelle_pose_from_lidar_transform();
    void reset_marker_interpolation(
        SphericalPoseMarker& marker,
        MarkerInterpolationState& state
    );
    void interpolate_marker_pose(
        SphericalPoseMarker& marker,
        MarkerInterpolationState& state,
        const NonholonomicPos& target,
        std::chrono::steady_clock::time_point now,
        bool force_new_sample = false
    );
    std::vector<LineInstance> make_path_lines(const std::vector<NonholonomicPos>& path, bool override_color = false);
    std::vector<LineInstance> make_path_lines(const std::vector<glm::vec3>& path);
    std::vector<LineInstance> make_path_lines(const std::vector<glm::ivec3>& path);
    std::vector<LineInstance> make_waypoint_path_lines(const std::vector<Celeris::Waypoint>& waypoints);
    void update_waypoint_path_visualization();
};
