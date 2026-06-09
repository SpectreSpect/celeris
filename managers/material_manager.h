#pragma once

#include <cstdint>

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/descriptor_set/descriptor_pool_builder.h"
#include "../vulkan_self/descriptor_set/descriptor_pool.h"
#include "../vulkan_self/pass/material_pass/material_pass.h"
#include "../vulkan_self/pass/instance/slot_pass_instance.h"

class VulkanEngine;
class ShaderManager;
class FrameResources;
class MaterialPassBuilder;
class VulkanShaderModule;
class VulkanTexture2D;
class Cubemap;
class CubemapArray;

struct PBRVertex {
    glm::vec4 position;
    glm::vec4 normal;
    glm::vec2 uv;
    glm::vec4 tangent;
};

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
    MaterialPass voxel_mesh_mp;
    MaterialPass voxel_pbr_mp;

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
    MaterialPass create_voxel_mesh_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                   const VulkanShaderModule& vs, const VulkanShaderModule& fs);
    MaterialPass create_voxel_pbr_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                   const VulkanShaderModule& vs, const VulkanShaderModule& fs);
        
    SlotPassInstance create_blinn_phong_material(VulkanEngine& engine, VulkanTexture2D& albedo);
    SlotPassInstance create_skybox_material(VulkanEngine& engine, Cubemap& skybox_cubemap);
    SlotPassInstance create_pbr_material(VulkanEngine& engine, CubemapArray& irradiance_maps, CubemapArray& prefilter_maps, VulkanTexture2D& brdf_lut);
    SlotPassInstance create_voxel_pbr_material(VulkanEngine& engine, CubemapArray& irradiance_maps, CubemapArray& prefilter_maps, VulkanTexture2D& brdf_lut);
    
private:
    DescriptorPool m_pool;
    static constexpr uint32_t m_max_material_instances = 256;
};
