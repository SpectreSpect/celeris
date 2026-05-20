#include "blin_phong_material_layout.h"

DescriptorSetLayoutBuilder BlinPhongMaterialLayout::create_builder() {
    DescriptorSetLayoutBuilder builder;

    builder
        .add_uniform_buffer(0, ShaderStages::fragment)
        .add_storage_buffer(1, ShaderStages::fragment);

    return builder;
}

BlinPhongMaterialLayout::BlinPhongMaterialLayout(const VulkanDevice& device)
    : MaterialLayout(device, create_builder()) {}