#pragma once
#include "voxel_engine/voxel.h"

class IGridable {
public:
    virtual void set_voxels(const std::vector<Voxel>& voxels, const std::vector<glm::ivec3>& positions) = 0;
    virtual void set_voxel(const Voxel& voxel, glm::ivec3 position) = 0;
    virtual Voxel get_voxel(glm::ivec3 position) const = 0;
};