#pragma once

#include "../logger/logger_header.h"
#include "../descriptor_set/descriptor_set.h"
#include "../descriptor_set/descriptor_pool.h"
#include "material_layout.h"
#include "material_pass.h"

class MaterialInstanceTemp {
public:
    _XCLASS_NAME(MaterialInstanceTemp);

    MaterialInstanceTemp(DescriptorPool& pool, MaterialPass& pass) 
        :   descriptor_set(pool.allocate_set(pass.descriptor_set_layout())),
            m_pass(pass) {};
    
    void bind(VulkanCommandBuffer& command_buffer);

    DescriptorSet descriptor_set;
    MaterialPass& m_pass;
// protected:
};