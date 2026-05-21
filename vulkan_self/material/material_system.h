#pragma once

#include <glm/glm.hpp>

#include "../logger/logger_header.h"
#include "../descriptor_set/descriptor_pool.h"
#include "blin_phong_material_layout.h"

class MaterialInstance;
class VulkanBuffer;

class MaterialSystem {
public:
    _XCLASS_NAME(MaterialLayout);

    explicit MaterialSystem(VulkanDevice& device);

    MaterialInstance create_blin_phong_material(VulkanBuffer& blin_phong_uniform, glm::vec4 albedo);

    BlinPhongMaterialLayout m_blin_phong_layout;
    DescriptorPool m_descriptor_pool;

private:
    

    DescriptorPool build_descriptor_pool(VulkanDevice& device);
};