#include "unlit_material_instance.h"

UnlitMaterialInstance::UnlitMaterialInstance(VulkanEngine& engine, DescriptorPool& descriptor_pool, MaterialPass& unlit_material_pass) 
    :   MaterialInstanceTemp(descriptor_pool, unlit_material_pass), 
        m_unifrom_buffer(VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(UnlitUniform))) {
    descriptor_set.write_uniform_buffer(0, m_unifrom_buffer);
}

void UnlitMaterialInstance::set_color(glm::vec4 color) {
    m_uniform_data.color = color;
    m_unifrom_buffer.upload(&m_uniform_data, sizeof(UnlitUniform));
}