#pragma once
#include "material_instance_temp.h"
#include "material_pass.h"
#include "../vulkan_buffer.h"
#include "../vulkan_engine.h"

class BlinnPhongMaterialInstance : public MaterialInstanceTemp {
public:
    struct BlinnPhongUniform {
        glm::vec4 color;
    };

    explicit BlinnPhongMaterialInstance(VulkanEngine& engine, DescriptorPool& descriptor_pool, MaterialPass& blinn_phong_material_pass);

    void set_color(glm::vec4 color);

private:
    BlinnPhongUniform m_uniform_data;
    VulkanBuffer m_unifrom_buffer;
};