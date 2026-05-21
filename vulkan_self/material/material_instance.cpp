#include "material_instance.h"

void MaterialInstance::bind(VulkanCommandBuffer& command_buffer, Pipeline& pipeline, uint32_t set_binding) {
    descriptor_set.bind(command_buffer, pipeline, set_binding);
}