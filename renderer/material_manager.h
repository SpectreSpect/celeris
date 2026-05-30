#pragma once

#include <cstdint>

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/descriptor_set/descriptor_pool_builder.h"
#include "../vulkan_self/descriptor_set/descriptor_pool.h"
#include "../vulkan_self/material/material_pass.h"

class VulkanEngine;
class ShaderManager;
class FrameResources;
class MaterialPassBuilder;
class VulkanShaderModule;
class MaterialInstance;
class VulkanTexture2D;
class Cubemap;

class MaterialManager {
public:
    _XCLASS_NAME(MaterialManager);

private:
    DescriptorPoolBuilder m_pool_builder;

public:
    MaterialPass blin_phong_mp;
    MaterialPass unlit_mp;
    MaterialPass point_mp;
    MaterialPass skybox_mp;
    MaterialPass pbr_mp;

    MaterialManager(VulkanEngine& engine, ShaderManager& shader_manager, FrameResources& frame_resources);

    DescriptorPool& descriptor_pool() noexcept;

    DescriptorPoolBuilder create_pool_builder();
    
    MaterialPass create_pass(
        VulkanEngine& engine,
        MaterialPassBuilder& builder,
        const VulkanShaderModule& vs,
        const VulkanShaderModule& fs
    );
                                              
    MaterialPass create_blin_phong_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                        const VulkanShaderModule& vs, const VulkanShaderModule& fs);
    MaterialPass create_unlit_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                   const VulkanShaderModule& vs, const VulkanShaderModule& fs);
    MaterialPass create_point_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                   const VulkanShaderModule& vs, const VulkanShaderModule& fs);
    MaterialPass create_skybox_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                   const VulkanShaderModule& vs, const VulkanShaderModule& fs);
    MaterialPass create_pbr_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                   const VulkanShaderModule& vs, const VulkanShaderModule& fs);
        
    MaterialInstance create_blinn_phong_material(VulkanEngine& engine, VulkanTexture2D& albedo);
    MaterialInstance create_skybox_material(VulkanEngine& engine, Cubemap& skybox_cubemap);
    MaterialInstance create_pbr_material(VulkanEngine& engine, Cubemap& irradiance_map, Cubemap& prefilter_map, VulkanTexture2D& brdf_lut);
    
private:
    DescriptorPool m_pool;
    static constexpr uint32_t m_max_material_instances = 256;
};
