#pragma once
#include "material_instance_temp.h"
#include "material_pass.h"
#include "../vulkan_buffer.h"
#include "../vulkan_engine.h"

class UnlitMaterialInstance : public MaterialInstanceTemp {
public:
    struct UnlitUniform {
        glm::vec4 color;
    };

    explicit UnlitMaterialInstance(VulkanEngine& engine, DescriptorPool& descriptor_pool, MaterialPass& unlit_material_pass);

    void set_color(glm::vec4 color);

private:
    UnlitUniform m_uniform_data;
    VulkanBuffer m_unifrom_buffer;
};