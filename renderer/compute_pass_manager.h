#pragma once

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/compute/compute_pass.h"
#include "../vulkan_self/descriptor_set/descriptor_pool.h"
#include "../vulkan_self/descriptor_set/descriptor_pool_builder.h"

class VulkanDevice;
class VulkanShaderModule;
class ShaderManager;

class ComputePassManager {
public:
    _XCLASS_NAME(ComputePassManager);

private:
    DescriptorPoolBuilder m_pool_builder;

public:
    ComputePass test_compute_pass;
    ComputePass gicp_cp;
    ComputePass point_voxel_map_insert_cp;
    ComputePass reset_voxel_point_map_cp;
    ComputePass gicp_reduce_cp;
    ComputePass build_cluster_light_lists_cp;

    ComputePass equirect_to_cubemap_cp;
    ComputePass brdf_lut_cp;
    ComputePass prefilter_map_cp;
    ComputePass irradiance_map_cp;

    ComputePassManager(VulkanDevice& device, ShaderManager& shader_manager);

    DescriptorPool& descriptor_pool() noexcept;

    ComputePass create_pass(VulkanDevice& device, ComputePassBuilder& builder);

    ComputePass create_test_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_gicp_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_point_voxel_map_insert_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_reset_voxel_point_map_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_gicp_reduce_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_build_cluster_light_lists_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_equirect_to_cubemap_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_brdf_lut_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_prefilter_map_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_irradiance_map_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);

private:
    DescriptorPool m_pool;
    static constexpr uint32_t m_max_compute_pass_instances = 256;
};
