#include "material_instance_temp.h"

void MaterialInstanceTemp::bind(VulkanCommandBuffer& command_buffer) {
    m_pass.pipeline().bind(command_buffer);
    descriptor_set.bind(command_buffer, m_pass.pipeline(), 0);
}