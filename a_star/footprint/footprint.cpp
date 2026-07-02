#include "footprint.h"

#include "../occupancy_grid_3d.h"

#include <cmath>
#include <utility>

#include <glm/ext/vector_int3.hpp>

Footprint::Footprint(
    OccupancyGrid3D& occupancy_grid,
    const VehicleGeometry& vehicle_geometry,
    uint32_t sample_count,
    uint32_t horizontal_inflation_size,
    uint32_t vertical_inflation_size)
    :   m_vehicle_geometry(vehicle_geometry),
        m_sample_count(sample_count),
        m_horizontal_inflation_size(horizontal_inflation_size),
        m_vertical_inflation_size(vertical_inflation_size),
        m_grid(&occupancy_grid) {}

bool Footprint::is_passible(const NonholonomicPos& rear_axle_pos) {
    return sample(rear_axle_pos).is_passible;
}

Footprint::SampleResult Footprint::sample(const NonholonomicPos& rear_axle_pos) {
    std::vector<glm::vec3> positions = sample_raw_positions(rear_axle_pos);

    std::vector<glm::ivec3> ground_positions;
    const bool is_passible =
        m_grid->adjust_to_ground(positions, 1, 1, 2) &&
        m_grid->get_ground_positions(positions, ground_positions, 1, 1, 2);

    return SampleResult{
        .positions = std::move(positions),
        .is_passible = is_passible
    };
}

Footprint::PathResult Footprint::evaluate_path(
    const std::vector<NonholonomicPos>& path,
    int max_step_up,
    int max_drop,
    int max_y_diff,
    bool allow_flying_over_precepices)
{
    PathResult result;
    result.path = path;

    result.is_passible =
        m_grid->adjust_to_ground(
            result.path,
            max_step_up,
            max_drop,
            max_y_diff,
            allow_flying_over_precepices) &&
        m_grid->get_ground_positions(
            result.path,
            result.ground_positions,
            max_step_up,
            max_drop,
            max_y_diff,
            allow_flying_over_precepices);

    if (!result.is_passible) {
        return result;
    }

    for (const NonholonomicPos& pose : result.path) {
        if (!is_passible(pose)) {
            result.is_passible = false;
            return result;
        }
    }

    return result;
}

std::vector<glm::vec3> Footprint::sample_raw_positions(const NonholonomicPos& rear_axle_pos) const {
    glm::vec3 voxel_size = m_grid->voxel_size();

    glm::vec3 dir(std::cos(rear_axle_pos.theta), 0.0f, std::sin(rear_axle_pos.theta));
    glm::vec3 origin = rear_axle_pos.pos + dir * -m_vehicle_geometry.rear_axle_from_rear;
    float first_pos_offset = static_cast<float>(m_horizontal_inflation_size) * voxel_size.x;
    float place_length = m_vehicle_geometry.size.z - first_pos_offset * 2.0f;
    float step_size = place_length / static_cast<float>(m_sample_count - 1);

    std::vector<glm::vec3> positions;
    positions.reserve(m_sample_count);
    for (uint32_t i = 0; i < m_sample_count; i++) {
        positions.push_back(origin + dir * (first_pos_offset + step_size * i));
    }

    return positions;
}

uint32_t Footprint::sample_count() const {
    return m_sample_count;
}

uint32_t Footprint::horizontal_inflation_size() const {
    return m_horizontal_inflation_size;
}

uint32_t Footprint::vertical_inflation_size() const {
    return m_vertical_inflation_size;
}
