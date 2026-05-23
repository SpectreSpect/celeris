#include "blinn_phong_material_instance.h"

#include "../vulkan_engine.h"
#include "material_pass.h"
#include "../image/vulkan_texture_2d.h"

BlinnPhongMaterialInstance::BlinnPhongMaterialInstance(
    VulkanEngine& engine, 
    DescriptorPool& descriptor_pool, 
    MaterialPass& blinn_phong_material_pass,
    VulkanTexture2D& texture)
    :   MaterialInstanceTemp(descriptor_pool, blinn_phong_material_pass), 
        m_unifrom_buffer(
            VulkanBuffer::create_host_visible_uniform_buffer(
                engine, 
                sizeof(BlinnPhongUniform)
            )
        )
{
    descriptor_set.write_uniform_buffer(0, m_unifrom_buffer);
    descriptor_set.write_texture(1, texture);
}

void BlinnPhongMaterialInstance::set_color(glm::vec4 color) {
    m_uniform_data.color = color;
    m_uniform_data.material = glm::vec4(0.1, 1, 0.5, 32.0);

    m_unifrom_buffer.upload(&m_uniform_data, sizeof(BlinnPhongUniform));
}