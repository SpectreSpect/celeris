#pragma once

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/pass/compute_pass/compute_pass.h"
#include "../vulkan_self/descriptor_set/descriptor_pool.h"
#include "../vulkan_self/descriptor_set/descriptor_pool_builder.h"

class VulkanDevice;
class VulkanShaderModule;
class ShaderManager;
class ComputePassBuilder;

class ComputePassManager {
public:
    _XCLASS_NAME(ComputePassManager);

private:
    DescriptorPoolBuilder m_pool_builder;

public:

    // General
    ComputePass fill_buffer_cp;

    // GICP
    ComputePass gicp_cp;
    ComputePass point_voxel_map_insert_cp;
    ComputePass reset_voxel_point_map_cp;
    ComputePass gicp_reduce_cp;

    // Lights
    ComputePass build_cluster_light_lists_cp;

    // Voxel grid
    ComputePass world_init_cp;
    // ComputePass apply_writes_to_world_cp;
    ComputePass mesh_pool_clear_cp;
    ComputePass mesh_pool_seed_cp;
    ComputePass dispatch_adapter_cp;
    ComputePass mesh_reset_cp;
    ComputePass mesh_count_cp;
    ComputePass mesh_alloc_cp;
    ComputePass verify_mesh_allocation_cp;
    ComputePass return_free_alloc_nodes_dispatch_adapter_cp;
    ComputePass return_free_alloc_nodes_cp;
    ComputePass mesh_emit_cp;
    ComputePass mesh_finalize_cp;
    ComputePass stream_select_chunks_cp;
    ComputePass insert_elements_to_voxel_write_list_cp;
    ComputePass add_voxel_write_list_counters_together_cp;

    // PBR
    ComputePass equirect_to_cubemap_cp;
    ComputePass brdf_lut_cp;
    ComputePass prefilter_map_cp;
    ComputePass irradiance_map_cp;

    ComputePassManager(VulkanDevice& device, ShaderManager& shader_manager);

    DescriptorPool& descriptor_pool() noexcept;

    ComputePass create_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module, ComputePassBuilder& builder);

    // General
    ComputePass create_fill_buffer_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);

    // GICP
    ComputePass create_gicp_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_point_voxel_map_insert_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_reset_voxel_point_map_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_gicp_reduce_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);

    // Lights
    ComputePass create_build_cluster_light_lists_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);

    // PBR
    ComputePass create_equirect_to_cubemap_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_brdf_lut_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_prefilter_map_pass(VulkanDevice& device, VulkanShaderModule& compute_shadder_module);
    ComputePass create_irradiance_map_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);

    // Voxel grid
    ComputePass create_world_init_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    // ComputePass create_apply_writes_to_world_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_mesh_pool_clear_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_mesh_pool_seed_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_dispatch_adapter_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_mesh_reset_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_mesh_count_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_mesh_alloc_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_verify_mesh_allocation_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_return_free_alloc_nodes_dispatch_adapter_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_return_free_alloc_nodes_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_mesh_emit_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_mesh_finalize_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);

    ComputePass create_stream_select_chunks_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_insert_elements_to_voxel_write_list_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_add_voxel_write_list_counters_together_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);

private:
    DescriptorPool m_pool;
    static constexpr uint32_t m_max_compute_pass_instances = 256;
};
