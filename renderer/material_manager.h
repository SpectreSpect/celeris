#pragma once

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/material/material_pass.h"
#include "../vulkan_self/descriptor_set/descriptor_pool.h"
#include "../vulkan_self/descriptor_set/descriptor_pool_builder.h"

class VulkanDevice;
class ShaderManager;
class FrameResources;

class MaterialManager {
public:
    _XCLASS_NAME(MaterialManager);

private:
    DescriptorPoolBuilder m_pool_builder;

public:
    MaterialPass blin_phong_mp;
    MaterialPass unlit_mp;

    MaterialManager(VulkanEngine& engine, ShaderManager& shader_manager, FrameResources& frame_resources);


    DescriptorPoolBuilder create_pool_builder();
    MaterialPass create_pass(VulkanEngine& engine, MaterialPassBuilder& builder, 
                                              const VulkanShaderModule& vs, const VulkanShaderModule& fs);
    DescriptorPool& descriptor_pool() noexcept;
                                              
    MaterialPass create_blin_phong_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                        const VulkanShaderModule& vs, const VulkanShaderModule& fs);
    MaterialPass create_unlit_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                   const VulkanShaderModule& vs, const VulkanShaderModule& fs);
private:
    DescriptorPool m_pool;
    static constexpr uint32_t m_max_material_instances = 256;
};