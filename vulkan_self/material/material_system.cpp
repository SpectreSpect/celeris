#include "material_system.h"

#include "../descriptor_set/descriptor_pool_builder.h"
#include "blin_phong_material_layout.h"
#include "../vulkan_buffer.h"
#include "material_instance.h"

MaterialSystem::MaterialSystem(VulkanDevice& device) 
    :   blin_phong_layout(device),
    m_descriptor_pool(build_descriptor_pool(device))  {
    
}

DescriptorPool MaterialSystem::build_descriptor_pool(VulkanDevice& device) {
    DescriptorPoolBuilder pool_builder;

    pool_builder.add_layout(blin_phong_layout.descriptor_set_layout_builder(), 256);

    return DescriptorPool(device, pool_builder);
}

MaterialInstance MaterialSystem::create_blin_phong_material(VulkanBuffer& blin_phong_uniform, glm::vec4 albedo) {
    blin_phong_uniform.upload(&albedo, sizeof(glm::vec4));

    
}