#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/vec3.hpp>

class PathPlanner;

class CollisionEscapeResolver {
public:
    struct Desc {
        size_t history_size = 8;
        uint32_t search_radius_voxels = 8;
        float clearance_voxels = 0.1f;
        uint32_t binary_search_iterations = 16;
        float boundary_epsilon_scale = 1e-3f;
        float sample_step_voxel_scale = 0.25f;
        float min_sample_step = 1e-4f;
        float min_segment_length = 1e-6f;
        float min_direction_length_sq = 1e-8f;
    };

    CollisionEscapeResolver(
        PathPlanner& path_planner,
        const Desc& desc
    );

    void push_out(glm::vec3& position);
    void reset();

private:
    PathPlanner* m_path_planner = nullptr;
    Desc m_desc;

    std::vector<glm::vec3> m_free_raw_position_history;
    glm::vec3 m_collision_surface_point{0.0f};
    bool m_has_collision_surface_point = false;

    void resolve_collision(glm::vec3& position);
    void remember_raw_position(glm::vec3 position);

    bool point_is_free(glm::vec3 point);
    glm::vec3 point_in_voxel_closest_to(
        glm::ivec3 voxel_position,
        glm::vec3 reference
    );
    float sample_step();
    float clearance_distance();
    float minimum_component(glm::vec3 value) const;
    float squared_length(glm::vec3 value) const;
    bool add_clearance(
        glm::vec3 position,
        glm::vec3 direction,
        glm::vec3& cleared_position
    );

    bool find_first_free_point_on_segment(
        glm::vec3 from,
        glm::vec3 to,
        glm::vec3& free_point
    );
    bool find_collision_surface_point(
        glm::vec3 position,
        glm::vec3& surface_point
    );
    bool find_escape_point(
        glm::vec3 position,
        glm::vec3 direction,
        glm::vec3& resolved_position
    );
};
