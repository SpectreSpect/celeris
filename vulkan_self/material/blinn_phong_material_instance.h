#pragma once
#include "material_instance_temp.h"
#include "../vulkan_buffer.h"

class VulkanEngine;
class MaterialPass;
class DescriptorPool;
class VulkanTexture2D;

class BlinnPhongMaterialInstance : public MaterialInstanceTemp {
public:

    explicit BlinnPhongMaterialInstance(
        VulkanEngine& engine,
        DescriptorPool& descriptor_pool,
        MaterialPass& blinn_phong_material_pass,
        VulkanTexture2D& texture
    );

    void set_color(glm::vec4 color);

private:
    struct BlinnPhongUniform {
        glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
        glm::vec4 material = {1.0f, 1.0f, 1.0f, 1.0f};
    };

private:
    BlinnPhongUniform m_uniform_data;
    VulkanBuffer m_unifrom_buffer;
};
