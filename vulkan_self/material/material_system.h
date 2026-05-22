#pragma once

#include <glm/glm.hpp>

#include "../logger/logger_header.h"
#include "../descriptor_set/descriptor_pool.h"
#include "blin_phong_material_layout.h"
#include "../vulkan_buffer.h"
#include "../descriptor_set/descriptor_pool.h"
#include "material_instance.h"
#include "material_pass.h"
#include "../../renderer/transform_push_constants.h"
#include "../vulkan_engine.h"

class MaterialSystem {
public:
    _XCLASS_NAME(MaterialLayout);

    explicit MaterialSystem(VulkanDevice& device);

    MaterialInstance create_blin_phong_material(VulkanBuffer& blin_phong_uniform, glm::vec4 albedo);

    static MaterialPass create_blin_phong_pass(VulkanEngine& engine,
                                               const DescriptorSetLayout& frame_resources_descriptor_layout,
                                               const VulkanShaderModule& vertex_shader,
                                               const VulkanShaderModule& fragment_shader);
    
    static MaterialPass create_unlit_pass(VulkanEngine& engine,
                                               const DescriptorSetLayout& frame_resources_descriptor_layout,
                                               const VulkanShaderModule& vertex_shader,
                                               const VulkanShaderModule& fragment_shader);

    BlinPhongMaterialLayout m_blin_phong_layout;
    DescriptorPool m_descriptor_pool;

private:
    DescriptorPool build_descriptor_pool(VulkanDevice& device);
};