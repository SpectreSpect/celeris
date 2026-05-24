#pragma once

#include "../logger/logger_header.h"
#include "../descriptor_set/descriptor_set.h"
#include "../descriptor_set/descriptor_pool.h"
#include "material_layout.h"
#include "material_pass.h"
#include "material_buffer.h"

class MaterialInstance {
public:
    _XCLASS_NAME(MaterialInstance);

    
    MaterialInstance(VulkanEngine& engine, DescriptorPool& pool, MaterialPass& pass, uint32_t material_data_size) 
        :   descriptor_set(pool.allocate_set(pass.descriptor_set_layout())),
            m_pass(pass),
            material_buffer(engine, 1000, material_data_size) {
        descriptor_set.write_storage_buffer(0, material_buffer.gpu_buffer());
        };
    
    void bind(VulkanCommandBuffer& command_buffer) {
        m_pass.pipeline().bind(command_buffer);
        descriptor_set.bind(command_buffer, m_pass.pipeline(), 0);
    }
    
    MaterialBuffer material_buffer;
    DescriptorSet descriptor_set;
    MaterialPass& m_pass;
};