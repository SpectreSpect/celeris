#pragma once
#include "gridable.h"
#include "buffer_object.h"

class IGridableGPU : public IGridable {
public:
    virtual void set_voxels(const BufferObject& voxel_write_list_src) = 0;
};