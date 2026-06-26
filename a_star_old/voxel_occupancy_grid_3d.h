#include "occupancy_grid_3d.h"
#include "../voxel_engine/voxel_grid.h"

class VoxelOccupancyGrid3D : public OccupancyGrid3D{
public:
    VoxelGrid* voxel_grid;
    VoxelOccupancyGrid3D(VoxelGrid* voxel_grid);

    // void set_cell(glm::ivec3 pos, OccupancyCell* occupancy_cell) override;
    OccupancyCell get_cell(glm::ivec3 pos) override;
    // bool adjust_to_ground(glm::vec3& pos, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1) override;
    // bool adjust_to_ground(std::vector<glm::vec3>& pos, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1) override;
    // bool adjust_to_ground(glm::vec3& pos, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1) override;
    // bool get_ground_positions(glm::vec3 pos1, glm::vec3 pos2, std::vector<glm::ivec3>& output, int max_step_up = 500, int max_drop = 500, int max_y_diff = -1) override;
};