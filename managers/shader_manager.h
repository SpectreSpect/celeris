#pragma once

#include "../vulkan_self/vulkan_shader_module.h"

class VulkanDevice;

class ShaderManager {
public:
    ShaderManager(VulkanDevice& device);

    VulkanShaderModule blinn_phong_vs;
    VulkanShaderModule blinn_phong_fs;

    VulkanShaderModule unlit_vs;
    VulkanShaderModule unlit_fs;

    VulkanShaderModule point_vs;
    VulkanShaderModule point_fs;
    
    VulkanShaderModule skybox_vs;
    VulkanShaderModule skybox_fs;

    VulkanShaderModule pbr_vs;
    VulkanShaderModule pbr_fs;

    // Compute shaders
    // General
    VulkanShaderModule fill_buffer_cs;

    // GICP
    VulkanShaderModule gicp_step_cs;
    VulkanShaderModule insert_points_into_voxel_map_cs;
    VulkanShaderModule reset_point_voxel_map_cs;
    VulkanShaderModule gicp_reduce_cs;
    
    // Lights
    VulkanShaderModule build_cluster_light_lists_cs;

    // Voxel grid
    VulkanShaderModule world_init_cs;
    // VulkanShaderModule apply_writes_to_world_cs;
    VulkanShaderModule mesh_pool_clear_cs;
    VulkanShaderModule mesh_pool_seed_cs;
    VulkanShaderModule dispatch_adapter_cs;
    VulkanShaderModule mesh_reset_cs;
    VulkanShaderModule mesh_count_cs;
    VulkanShaderModule mesh_alloc_cs;
    VulkanShaderModule verify_mesh_allocation_cs;
    VulkanShaderModule return_free_alloc_nodes_dispatch_adapter_cs;
    VulkanShaderModule return_free_alloc_nodes_cs;
    VulkanShaderModule mesh_emit_cs;
    VulkanShaderModule mesh_finalize_cs;
    VulkanShaderModule reset_dirty_count_cs;
    VulkanShaderModule stream_select_chunks_cs;
    VulkanShaderModule insert_elements_to_voxel_write_list_cs;
    VulkanShaderModule add_voxel_write_list_counters_together_cs;
    VulkanShaderModule mark_write_chunks_to_generate_cs;
    VulkanShaderModule stream_generate_terrain_cs;
    VulkanShaderModule write_voxels_to_grid_cs;
    VulkanShaderModule evict_buckets_build_cs;
    VulkanShaderModule evict_low_priority_dispatch_adapter_cs;
    VulkanShaderModule evict_low_priority_cs;
    VulkanShaderModule free_evicted_chunks_mesh_cs;
    VulkanShaderModule reset_evicted_list_and_buckets_cs;
    VulkanShaderModule hash_table_conditional_dispatch_adapter_cs;
    VulkanShaderModule clear_chunk_hash_table_cs;
    VulkanShaderModule fill_chunk_hash_table_cs;

    // PBR
    VulkanShaderModule equirect_to_cubemap_cs;
    VulkanShaderModule brdf_lut_cs;
    VulkanShaderModule generate_prefilter_map_cs;
    VulkanShaderModule generate_irradiance_map_cs;
};
