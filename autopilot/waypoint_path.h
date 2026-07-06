#pragma once

#include "spherical_pose_marker.h"
#include "../a_star/a_star_structures.h"
#include "../renderer/lines/line_cloud.h"
#include "../renderer/lines/line_instance.h"
#include "../renderer/render_object.h"
#include "../renderer/scene_object.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

class MaterialInstanceManager;
class Mesh;
class MeshManager;
class SlotPassInstance;
class VulkanEngine;

class WaypointPath : public SceneObject {
public:
    _XCHILD_NAME(WaypointPath);

    struct Waypoint {
        glm::vec4 position{0.0f, 0.0f, 0.0f, 1.0f};
        std::optional<float> theta;

        glm::vec3 world_position() const noexcept {
            return glm::vec3(position);
        }

        bool directional() const noexcept {
            return theta.has_value();
        }
    };

    WaypointPath(
        VulkanEngine& engine,
        MeshManager& mesh_manager,
        MaterialInstanceManager& material_instance_manager,
        uint32_t max_line_count = 20000,
        float skybox_exposure = 1.8f
    );

    void add_waypoint(glm::vec3 position);
    void add_waypoint(const NonholonomicPos& position);
    void delete_last_waypoint();
    void clear();
    void save(const std::filesystem::path& path) const;
    void load(const std::filesystem::path& path);
    void set_first_visible_waypoint_index(size_t index);

    const std::vector<Waypoint>& waypoints() const noexcept;
    size_t directional_waypoint_count() const noexcept;
    size_t first_visible_waypoint_index() const noexcept;

private:
    uint32_t m_max_line_count = 0;
    float m_skybox_exposure = 1.8f;

    MeshManager* m_mesh_manager = nullptr;
    MaterialInstanceManager* m_material_instance_manager = nullptr;
    Mesh* m_sphere_mesh = nullptr;
    SlotPassInstance* m_sphere_material = nullptr;

    LineCloud m_line_cloud;
    std::vector<Waypoint> m_waypoints;
    size_t m_first_visible_waypoint_index = 0;
    std::vector<std::unique_ptr<RenderObject>> m_spheres;
    std::vector<std::unique_ptr<SphericalPoseMarker>> m_pose_markers;

    glm::vec3 marker_vertical_offset() const noexcept;
    std::vector<LineInstance> make_lines() const;
    void refresh_visualization();
    void set_marker_pose(SphericalPoseMarker& marker, const NonholonomicPos& position);
    void create_sphere();
    void create_pose_marker();
};
