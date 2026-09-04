#include "collision_escape_resolver.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/vector_relational.hpp>

#include "../path_planner.h"
#include "../../vulkan_self/logger/logger_header.h"

CollisionEscapeResolver::CollisionEscapeResolver(
    PathPlanner& path_planner,
    const CollisionEscapeResolverDesc& desc)
    :   m_path_planner(&path_planner),
        m_desc(desc) {
    logger().check(
        m_desc.binary_search_iterations > 0,
        "Collision binary search iteration count must be greater than zero"
    );
    logger().check(
        m_desc.voxel_boundary_epsilon_scale > 0.0f &&
            m_desc.voxel_boundary_epsilon_scale < 0.5f,
        "Collision voxel boundary epsilon scale must be between zero and one half"
    );
    logger().check(
        m_desc.sample_step_voxel_scale > 0.0f,
        "Collision sample step voxel scale must be greater than zero"
    );
    logger().check(
        m_desc.minimum_sample_step > 0.0f,
        "Collision minimum sample step must be greater than zero"
    );
    logger().check(
        m_desc.minimum_segment_length >= 0.0f,
        "Collision minimum segment length must not be negative"
    );
    logger().check(
        m_desc.minimum_direction_length_squared >= 0.0f,
        "Collision minimum direction length squared must not be negative"
    );
    logger().check(
        m_desc.collision_escape_search_radius_voxels <=
            static_cast<uint32_t>(std::numeric_limits<int>::max()),
        "Collision escape search radius must fit in an int"
    );
    logger().check(
        std::isfinite(m_desc.collision_clearance_voxels) &&
            m_desc.collision_clearance_voxels >= 0.0f,
        "Collision clearance in voxels must be finite and not negative"
    );
}

void CollisionEscapeResolver::push_out(glm::vec3& position) {
    const glm::vec3 raw_position = position;

    resolve_collision(position);
    remember_raw_position(raw_position);
}

void CollisionEscapeResolver::reset() {
    m_free_raw_position_history.clear();
    m_collision_surface_point = glm::vec3(0.0f);
    m_has_collision_surface_point = false;
}

void CollisionEscapeResolver::resolve_collision(glm::vec3& position) {
    if (point_is_free(position)) {
        return;
    }

    if (m_free_raw_position_history.empty() && !m_has_collision_surface_point) {
        return;
    }

    bool has_fallback_surface_point = false;
    glm::vec3 fallback_surface_point(0.0f);
    glm::vec3 escape_direction(0.0f);

    if (m_has_collision_surface_point) {
        escape_direction = m_collision_surface_point - position;
    } else {
        if (!find_collision_surface_point(position, m_collision_surface_point)) {
            return;
        }

        m_has_collision_surface_point = true;
        fallback_surface_point = m_collision_surface_point;
        has_fallback_surface_point = true;
        escape_direction = position - m_collision_surface_point;
    }

    glm::vec3 resolved_position(0.0f);
    if (find_escape_point(position, escape_direction, resolved_position)) {
        position = resolved_position;
        m_collision_surface_point = resolved_position;
        m_has_collision_surface_point = true;
        return;
    }

    if (find_collision_surface_point(position, resolved_position)) {
        position = resolved_position;
        m_collision_surface_point = resolved_position;
        m_has_collision_surface_point = true;
        return;
    }

    if (m_has_collision_surface_point && point_is_free(m_collision_surface_point)) {
        position = m_collision_surface_point;
        return;
    }

    if (has_fallback_surface_point && point_is_free(fallback_surface_point)) {
        position = fallback_surface_point;
        m_collision_surface_point = fallback_surface_point;
        m_has_collision_surface_point = true;
        return;
    }

    logger().log_error()
        << "Failed to resolve collision along previous path. "
        << "Leaving position unchanged.\n";
}

void CollisionEscapeResolver::remember_raw_position(glm::vec3 position) {
    if (m_desc.collision_history_size == 0) {
        reset();
        return;
    }

    if (!point_is_free(position)) {
        return;
    }

    m_has_collision_surface_point = false;

    m_free_raw_position_history.insert(
        m_free_raw_position_history.begin(),
        position
    );

    if (m_free_raw_position_history.size() > m_desc.collision_history_size) {
        m_free_raw_position_history.resize(m_desc.collision_history_size);
    }
}

bool CollisionEscapeResolver::point_is_free(glm::vec3 point) {
    logger().check(m_path_planner, "Path planner was null");
    return !m_path_planner->request_is_solid_world(point);
}

glm::vec3 CollisionEscapeResolver::point_in_voxel_closest_to(
    glm::ivec3 voxel_position,
    glm::vec3 reference) {
    logger().check(m_path_planner, "Path planner was null");

    const glm::vec3 voxel_size = m_path_planner->request_voxel_size();
    const glm::vec3 voxel_min =
        m_path_planner->request_voxel_to_world_pos(voxel_position);
    const glm::vec3 voxel_max = voxel_min + voxel_size;
    const glm::vec3 epsilon =
        voxel_size * m_desc.voxel_boundary_epsilon_scale;

    return glm::clamp(reference, voxel_min + epsilon, voxel_max - epsilon);
}

float CollisionEscapeResolver::sample_step() {
    logger().check(m_path_planner, "Path planner was null");

    const glm::vec3 voxel_size = m_path_planner->request_voxel_size();
    logger().check(
        glm::all(glm::greaterThan(voxel_size, glm::vec3(0.0f))),
        "Voxel size must be greater than zero"
    );

    return std::max(
        minimum_component(voxel_size) * m_desc.sample_step_voxel_scale,
        m_desc.minimum_sample_step
    );
}

float CollisionEscapeResolver::clearance_distance() {
    logger().check(m_path_planner, "Path planner was null");

    const glm::vec3 voxel_size = m_path_planner->request_voxel_size();
    logger().check(
        glm::all(glm::greaterThan(voxel_size, glm::vec3(0.0f))),
        "Voxel size must be greater than zero"
    );

    return minimum_component(voxel_size) *
        m_desc.collision_clearance_voxels;
}

float CollisionEscapeResolver::minimum_component(glm::vec3 value) const {
    return std::min({value.x, value.y, value.z});
}

float CollisionEscapeResolver::squared_length(glm::vec3 value) const {
    return glm::dot(value, value);
}

bool CollisionEscapeResolver::add_clearance(
    glm::vec3 position,
    glm::vec3 direction,
    glm::vec3& cleared_position) {
    const float distance = clearance_distance();
    if (distance == 0.0f) {
        cleared_position = position;
        return true;
    }

    const float direction_length_squared = squared_length(direction);
    if (!std::isfinite(direction_length_squared) ||
        direction_length_squared <= m_desc.minimum_direction_length_squared) {
        return false;
    }

    const glm::vec3 direction_normalized =
        direction / std::sqrt(direction_length_squared);
    const glm::vec3 candidate = position + direction_normalized * distance;

    if (!point_is_free(candidate)) {
        return false;
    }

    cleared_position = candidate;
    return true;
}

bool CollisionEscapeResolver::find_first_free_point_on_segment(
    glm::vec3 from,
    glm::vec3 to,
    glm::vec3& free_point) {
    const glm::vec3 segment = to - from;
    const float segment_length = glm::length(segment);

    if (!std::isfinite(segment_length) ||
        segment_length <= m_desc.minimum_segment_length) {
        if (point_is_free(to)) {
            free_point = to;
            return true;
        }

        return false;
    }

    const float collision_sample_step = sample_step();
    const int sample_count = std::max(
        1,
        static_cast<int>(std::ceil(segment_length / collision_sample_step))
    );

    glm::vec3 last_blocked = from;

    for (int sample_id = 1; sample_id <= sample_count; ++sample_id) {
        const float interpolation =
            static_cast<float>(sample_id) / static_cast<float>(sample_count);
        const glm::vec3 candidate = glm::mix(from, to, interpolation);

        if (!point_is_free(candidate)) {
            last_blocked = candidate;
            continue;
        }

        glm::vec3 blocked = last_blocked;
        glm::vec3 free = candidate;

        for (uint32_t iteration = 0;
             iteration < m_desc.binary_search_iterations;
             ++iteration) {
            const glm::vec3 middle = (blocked + free) * 0.5f;

            if (point_is_free(middle)) {
                free = middle;
            } else {
                blocked = middle;
            }
        }

        if (!add_clearance(free, segment, free_point)) {
            free_point = free;
        }
        return true;
    }

    return false;
}

bool CollisionEscapeResolver::find_collision_surface_point(
    glm::vec3 position,
    glm::vec3& surface_point) {
    for (glm::vec3 previous_position : m_free_raw_position_history) {
        if (find_first_free_point_on_segment(
                position,
                previous_position,
                surface_point)) {
            return true;
        }
    }

    return false;
}

bool CollisionEscapeResolver::find_escape_point(
    glm::vec3 position,
    glm::vec3 direction,
    glm::vec3& resolved_position) {
    if (m_desc.collision_escape_search_radius_voxels == 0) {
        return false;
    }

    const float direction_length_squared = squared_length(direction);
    if (!std::isfinite(direction_length_squared) ||
        direction_length_squared <= m_desc.minimum_direction_length_squared) {
        return false;
    }

    const glm::vec3 direction_normalized =
        direction / std::sqrt(direction_length_squared);
    const glm::ivec3 center_voxel =
        m_path_planner->request_world_to_voxel_pos(position);
    const int radius = static_cast<int>(
        m_desc.collision_escape_search_radius_voxels
    );

    bool found = false;
    float best_distance_squared = std::numeric_limits<float>::infinity();
    float best_forward_projection = -std::numeric_limits<float>::infinity();
    glm::vec3 best_position(0.0f);

    for (int z = -radius; z <= radius; ++z) {
        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                const glm::ivec3 candidate_voxel =
                    center_voxel + glm::ivec3(x, y, z);
                if (m_path_planner->request_is_solid(candidate_voxel)) {
                    continue;
                }

                const glm::vec3 surface_position =
                    point_in_voxel_closest_to(candidate_voxel, position);
                const glm::vec3 surface_offset = surface_position - position;

                glm::vec3 candidate_position(0.0f);
                if (!add_clearance(
                        surface_position,
                        surface_offset,
                        candidate_position)) {
                    continue;
                }

                const glm::vec3 offset = candidate_position - position;
                const float candidate_distance_squared = squared_length(offset);

                if (candidate_distance_squared <=
                    m_desc.minimum_direction_length_squared) {
                    continue;
                }

                const float forward_projection =
                    glm::dot(offset, direction_normalized);
                if (forward_projection <= 0.0f) {
                    continue;
                }

                if (!found ||
                    candidate_distance_squared < best_distance_squared ||
                    (candidate_distance_squared == best_distance_squared &&
                     forward_projection > best_forward_projection)) {
                    found = true;
                    best_distance_squared = candidate_distance_squared;
                    best_forward_projection = forward_projection;
                    best_position = candidate_position;
                }
            }
        }
    }

    if (!found) {
        return false;
    }

    resolved_position = best_position;
    return true;
}
