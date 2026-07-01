#pragma once

#include "../../autopilot/vehicle_geometry.h"
#include "../a_star_structures.h"

#include <cstdint>
#include <vector>

#include <glm/vec3.hpp>

class OccupancyGrid3D;

class Footprint {
public:
    struct SampleResult {
        std::vector<glm::vec3> positions;
        bool is_passible = false;
    };

    struct PathResult {
        std::vector<NonholonomicPos> path;
        std::vector<glm::ivec3> ground_positions;
        bool is_passible = false;
    };

    Footprint(
        OccupancyGrid3D& occupancy_grid,
        const VehicleGeometry& vehicle_geometry,
        uint32_t sample_count = 5,
        uint32_t horizontal_inflation_size = 1,
        uint32_t vertical_inflation_size = 1
    );

    bool is_passible(const NonholonomicPos& rear_axle_pos);
    SampleResult sample(const NonholonomicPos& rear_axle_pos);
    PathResult evaluate_path(
        const std::vector<NonholonomicPos>& path,
        int max_step_up = 1,
        int max_drop = 1,
        int max_y_diff = 2,
        bool allow_flying_over_precepices = true
    );
    std::vector<glm::vec3> sample_raw_positions(const NonholonomicPos& rear_axle_pos) const;

    uint32_t sample_count() const;
    uint32_t horizontal_inflation_size() const;
    uint32_t vertical_inflation_size() const;

private:
    VehicleGeometry m_vehicle_geometry;
    uint32_t m_sample_count = 5;
    uint32_t m_horizontal_inflation_size = 0;
    uint32_t m_vertical_inflation_size = 0;
    OccupancyGrid3D* m_grid = nullptr;
};
