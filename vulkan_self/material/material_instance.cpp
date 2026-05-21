#include "material_instance.h"

void MaterialInstance::bind(VulkanCommandBuffer& command_buffer, Pipeline& pipeline) {
    descriptor_set.bind(command_buffer, pipeline);
}