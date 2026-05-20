#pragma once

#include "../logger/logger_header.h"
#include "../descriptor_set/descriptor_set.h"
#include "../descriptor_set/descriptor_pool.h"
#include "material_layout.h"

class MaterialInstance {
public:
    _XCLASS_NAME(MaterialLayout);

    MaterialInstance(DescriptorPool& pool, MaterialLayout& layout) 
        :   descriptor_set(pool.allocate_set(layout.ds_layout)) {};

protected:
    DescriptorSet descriptor_set;
};