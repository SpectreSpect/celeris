#include "occupancy_grid_3d.h"


bool OccupancyGrid3D::is_solid(glm::ivec3 pos) {
    return get_cell(pos).solid;
}

void OccupancyGrid3D::set_cell(glm::ivec3 pos, OccupancyCell occupancy_cell) {
    uint64_t key = pack_key(pos.x, pos.y, pos.z);
    occupancy_cells[key] = occupancy_cell;
}

OccupancyCell OccupancyGrid3D::get_cell(glm::ivec3 pos) {
    uint64_t key = pack_key(pos.x, pos.y, pos.z);
    auto it = occupancy_cells.find(key);
    // auto extra_it = extra_cells.find(key);

    // if (extra_it != extra_cells.end())
    //     return extra_it->second;

    if (it == occupancy_cells.end()) {
        OccupancyCell air = OccupancyCell();
        air.solid = false;
        return air;
    }
    
    return it->second;
}

// bool OccupancyGrid3D::get_ground_positions(glm::vec3 pos1, glm::vec3 pos2, std::vector<glm::ivec3>& output, int max_step_up, int max_drop, int max_y_diff) {
//     std::cout << "bool OccupancyGrid3D::get_ground_positions(glm::vec3 pos1, glm::vec3 pos2) is not implemented." << std::endl;
// }

// bool OccupancyGrid3D::adjust_to_ground(glm::vec3& pos, int max_step_up, int max_drop, int max_y_diff) {
//     auto solid = [&](const glm::ivec3& q) {
//         return get_cell(q).solid;
//     };

//     glm::ivec3 result = glm::ivec3(glm::floor(pos));

//     // 1) If we're inside solid, try stepping up
//     if (solid(result)) {
//         bool freed = false;
//         for (int k = 1; k <= max_step_up; ++k) {
//             glm::ivec3 up = result + glm::ivec3(0, k, 0);
//             if (!solid(up)) {
//                 result = up;
//                 freed = true;
//                 break;
//             }
//         }
//         if (!freed) return false;
//     }

//     // 2) Now find a y such that: current is empty AND below is solid
//     // (and don't drop more than max_drop)
//     for (int drop = 0; drop <= max_drop; ++drop) {
//         if (!solid(result) && solid(result + glm::ivec3(0, -1, 0))) {
//             pos.y = result.y;
//             return true;
//         }
            

//         // If we somehow are in solid, we're already too low → reject
//         // (or you could step up 1, but reject is safer)
//         if (solid(result))
//             return false;

//         result.y -= 1;
//     }
//     return false;
// }

// bool OccupancyGrid3D::adjust_to_ground(std::vector<glm::vec3>& pos, int max_step_up, int max_drop, int max_y_diff) {
//     std::cout << "bool adjust_to_ground(std::vector<glm::vec3>& pos, int max_step_up, int max_drop, int max_y_diff) is not implemented." << std::endl;
// }