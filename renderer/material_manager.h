#pragma once

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/material/material_pass.h"
#include "../vulkan_self/descriptor_set/descriptor_pool.h"
#include "../vulkan_self/descriptor_set/descriptor_pool_builder.h"

class VulkanDevice;
class ShaderManager;
class FrameResources;
class MaterialInstance;
class VulkanTexture2D;

struct BlinPhongMaterialData {
        glm::vec4 material;
        glm::vec4 color;
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

    MaterialManager(VulkanEngine& engine, ShaderManager& shader_manager, FrameResources& frame_resources);


    DescriptorPoolBuilder create_pool_builder();
    MaterialPass create_pass(VulkanEngine& engine, MaterialPassBuilder& builder, 
                                              const VulkanShaderModule& vs, const VulkanShaderModule& fs);
    DescriptorPool& descriptor_pool() noexcept;
                                              
    MaterialPass create_blin_phong_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                        const VulkanShaderModule& vs, const VulkanShaderModule& fs);
    MaterialPass create_unlit_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                   const VulkanShaderModule& vs, const VulkanShaderModule& fs);
    MaterialPass create_point_pass(VulkanEngine& engine, FrameResources& frame_resources, 
                                   const VulkanShaderModule& vs, const VulkanShaderModule& fs);
        
    MaterialInstance create_blinn_phong_material(VulkanEngine& engine, VulkanTexture2D& albedo);
private:
    DescriptorPool m_pool;
    static constexpr uint32_t m_max_material_instances = 256;
};