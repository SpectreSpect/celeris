#include "blinn_phong_material_instance.h"

BlinnPhongMaterialInstance::BlinnPhongMaterialInstance(VulkanEngine& engine, DescriptorPool& descriptor_pool, MaterialPass& blinn_phong_material_pass) 
    :   MaterialInstanceTemp(descriptor_pool, blinn_phong_material_pass), 
        m_unifrom_buffer(VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(BlinnPhongUniform))) {
    descriptor_set.write_uniform_buffer(0, m_unifrom_buffer);
}

void BlinnPhongMaterialInstance::set_color(glm::vec4 color) {
    m_uniform_data.color = color;
    
    m_unifrom_buffer.upload(&m_uniform_data, sizeof(BlinnPhongUniform));
}