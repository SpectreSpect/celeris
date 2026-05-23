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

    static MaterialPass create_blin_phong_pass(VulkanEngine& engine,
                                               const DescriptorSetLayout& frame_resources_descriptor_layout,
                                               const VulkanShaderModule& vertex_shader,
                                               const VulkanShaderModule& fragment_shader);
    
    static MaterialPass create_unlit_pass(VulkanEngine& engine,
                                               const DescriptorSetLayout& frame_resources_descriptor_layout,
                                               const VulkanShaderModule& vertex_shader,
                                               const VulkanShaderModule& fragment_shader);

    DescriptorPool& descriptor_pool() noexcept;
    
private:
    DescriptorPool m_descriptor_pool;

    DescriptorPool build_descriptor_pool(VulkanDevice& device);
};