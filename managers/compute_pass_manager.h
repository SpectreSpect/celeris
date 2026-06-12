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

    // Cloud to mesh
    ComputePass generate_mesh_cp;

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
    ComputePass reset_dirty_count_cp;
    ComputePass stream_select_chunks_cp;
    ComputePass insert_elements_to_voxel_write_list_cp;
    ComputePass add_voxel_write_list_counters_together_cp;
    ComputePass mark_write_chunks_to_generate_cp;
    ComputePass stream_generate_terrain_cp;
    ComputePass write_voxels_to_grid_cp;
    ComputePass evict_buckets_build_cp;
    ComputePass evict_low_priority_dispatch_adapter_cp;
    ComputePass evict_low_priority_cp;
    ComputePass build_indirect_cmds_cp;
    ComputePass free_evicted_chunks_mesh_cp;
    ComputePass reset_evicted_list_and_buckets_cp;
    ComputePass hash_table_conditional_dispatch_adapter_cp;
    ComputePass clear_chunk_hash_table_cp;
    ComputePass fill_chunk_hash_table_cp;
    ComputePass read_voxel_grid_chunk_cp;
    ComputePass voxel_writes_from_point_cloud_cp;

    // Voxelizator
    ComputePass alloc_active_chunk_triangles_cp;
    ComputePass fill_triangle_indices_cp;
    ComputePass mark_and_count_active_chunks_cp;
    ComputePass mark_and_count_fail_slots_cp;
    ComputePass reset_voxelize_pipeline_cp;
    ComputePass voxelize_triangles_cp;

    // Point cloud
    ComputePass normals_from_webots_lidar_point_cloud_cp;
    ComputePass remove_near_origin_lidar_points_cp;

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

    // Cloud to mesh
    ComputePass create_generate_mesh_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);

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
    ComputePass create_reset_dirty_count_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_stream_select_chunks_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_insert_elements_to_voxel_write_list_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_add_voxel_write_list_counters_together_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_mark_write_chunks_to_generate_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_stream_generate_terrain_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_write_voxels_to_grid_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_evict_buckets_build_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_evict_low_priority_dispatch_adapter_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_evict_low_priority_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_build_indirect_cmds_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_free_evicted_chunks_mesh_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_reset_evicted_list_and_buckets_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_hash_table_conditional_dispatch_adapter_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_clear_chunk_hash_table_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_fill_chunk_hash_table_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_read_voxel_grid_chunk_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_voxel_writes_from_point_cloud_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);

    // Voxelizator
    ComputePass create_alloc_active_chunk_triangles_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_fill_triangle_indices_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_mark_and_count_active_chunks_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_mark_and_count_fail_slots_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_reset_voxelize_pipeline_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_voxelize_triangles_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);

    // Point cloud
    ComputePass create_normals_from_webots_lidar_point_cloud_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);
    ComputePass create_remove_near_origin_lidar_points_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);

private:
    DescriptorPool m_pool;
    static constexpr uint32_t m_max_compute_pass_instances = 256;
};
