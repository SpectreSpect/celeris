#include "voxel_occupancy_grid_3d.h"

VoxelOccupancyGrid3D::VoxelOccupancyGrid3D(VoxelGrid* voxel_grid) {
    this->voxel_grid = voxel_grid;
}

// void VoxelOccupancyGrid3D::set_cell(glm::ivec3 pos, OccupancyCell* occupancy_cell) {

// }

OccupancyCell VoxelOccupancyGrid3D::get_cell(glm::ivec3 pos) {
    Voxel voxel = voxel_grid->get_voxel(pos);

    OccupancyCell occupancy_cell;
    occupancy_cell.solid = voxel.visible;
    occupancy_cell.curvature = voxel.curvature;
    // std::cout << voxel.curvature << std::endl;

    return occupancy_cell;
}

// bool VoxelOccupancyGrid3D::adjust_to_ground(glm::vec3& pos, int max_step_up, int max_drop, int max_y_diff) {
//     return voxel_grid->adjust_to_ground(pos, max_step_up, max_drop);
// }

// bool VoxelOccupancyGrid3D::adjust_to_ground(std::vector<glm::vec3>& pos, int max_step_up, int max_drop, int max_y_diff) {
//     return voxel_grid->adjust_to_ground(pos, max_step_up, max_drop, max_y_diff);
// }

// bool VoxelOccupancyGrid3D::get_ground_positions(glm::vec3 pos1, glm::vec3 pos2, std::vector<glm::ivec3>& output, int max_step_up, int max_drop, int max_y_diff) {
//     output = voxel_grid->line_intersects(pos1, pos2);
//     return voxel_grid->adjust_to_ground(output, max_step_up, max_drop, max_y_diff);
// }