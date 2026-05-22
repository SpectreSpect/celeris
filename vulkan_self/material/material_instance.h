#pragma once

#include "../logger/logger_header.h"
#include "../descriptor_set/descriptor_set.h"
#include "../descriptor_set/descriptor_pool.h"
#include "material_layout.h"

class MaterialInstance {
public:
    _XCLASS_NAME(MaterialInstance);

    MaterialInstance(DescriptorPool& pool, const MaterialLayout& layout) 
        :   descriptor_set(pool.allocate_set(layout.descriptor_set_layout())) {};
    
    void bind(VulkanCommandBuffer& command_buffer, Pipeline& pipeline, uint32_t set_binding);

    DescriptorSet descriptor_set;
};