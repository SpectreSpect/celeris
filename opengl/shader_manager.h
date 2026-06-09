#pragma once
#include <iostream>
#include <string>
#include <filesystem>

#include "compute_shader.h"
#include "vertex_shader.h"
#include "fragment_shader.h"
#include "compute_program.h"
#include <vector>

namespace fs = std::filesystem;

class ShaderManager {
public:
<<<<<<< HEAD:shader_manager.h
    // general
=======
    // default shaders
    VertexShader default_vertex_shader;
    FragmentShader default_fragment_shader;

    VertexShader default_line_vertex_shader;
    FragmentShader default_line_fragment_shader;

    VertexShader default_point_vertex_shader;
    FragmentShader default_point_fragment_shader;

    VertexShader default_cirlce_vertex_shader;

    VertexShader skybox_vs;
    FragmentShader skybox_fs;

    VertexShader equirect_to_cubemap_vs;
    FragmentShader equirect_to_cubemap_fs;

    VertexShader irradiance_vs;
    FragmentShader irradiance_fs;

    VertexShader prefilter_vs;
    FragmentShader prefilter_fs;

    VertexShader brdf_lut_vs;
    FragmentShader brdf_lut_fs;

    std::vector<std::filesystem::path> include_directories;
    // General
>>>>>>> origin/cloud-to-mesh:opengl/shader_manager.h
    ComputeShader dispatch_adapter_cs;
    ComputeShader clear_buffer_cs;
    ComputeProgram clear_buffer_prog;

    // voxelizaton
    ComputeShader reset_voxelize_pipeline_cs;
    ComputeShader mark_and_count_active_chunks_cs;
    ComputeShader alloc_active_chunk_triangles_cs;
    ComputeShader fill_triangle_indices_cs;
    ComputeShader voxelize_triangles_cs;


    // voxel_grid
    ComputeShader clear_chunks_cs;
    ComputeShader world_init_cs;
    ComputeShader apply_writes_to_world_cs;
    ComputeShader mesh_counters_reset_cs;
    ComputeShader mesh_reset_cs;
    ComputeShader mesh_count_cs;
    ComputeShader mesh_alloc_cs;
    ComputeShader mesh_emit_cs;
    ComputeShader mesh_finalize_cs;
    ComputeShader build_indirect_cmds_cs;
    ComputeShader reset_dirty_count_cs;
    ComputeShader evict_buckets_build_cs;
    ComputeShader evict_low_priority_cs;
    ComputeShader evict_low_priority_dispatch_adapter_cs;
    ComputeShader stream_select_chunks_cs;
    ComputeShader stream_generate_terrain_cs;
    ComputeShader mark_all_user_chunks_as_dirty_cs;
    ComputeShader mesh_pool_clear_cs;
    ComputeShader mesh_pool_seed_cs;
    ComputeShader verify_mesh_allocation_cs;
    ComputeShader return_free_alloc_nodes_cs;
    ComputeShader return_free_alloc_nodes_dispatch_adapter_cs;
    ComputeShader free_evicted_chunks_mesh_cs;
    ComputeShader fill_chunk_hash_table_cs;
    ComputeShader clear_chunk_hash_table_cs;
    ComputeShader reset_evicted_list_and_buckets_cs;
    ComputeShader hash_table_conditional_dispatch_adapter_cs;
    ComputeShader write_voxels_to_grid_cs;
    ComputeShader mark_write_chunks_to_generate_cs;
    ComputeShader insert_elements_to_voxel_write_list_cs;
    ComputeShader add_voxel_write_list_counters_together_cs;
    VertexShader voxel_mesh_vs;
    FragmentShader voxel_mesh_fs;

<<<<<<< HEAD:shader_manager.h
    ShaderManager(
        const std::filesystem::path& root_path,
        const std::vector<std::filesystem::path>* include_directories = nullptr,
        const std::filesystem::path* debug_root_path = nullptr
    );
=======
    ComputeShader light_incides_for_clusters_cs;
    ComputeShader test_hash_table_cs;
    ComputeShader add_point_cloud_to_map_cs;
    ComputeShader align_point_cloud_cs;
    

    std::vector<Shader*> shaders;

    ShaderManager() = default;
    ShaderManager(const std::string& root_path);
    ShaderManager(const std::filesystem::path& root_path, std::vector<std::filesystem::path> include_directories = std::vector<std::filesystem::path>());
>>>>>>> origin/cloud-to-mesh:opengl/shader_manager.h
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;
    ShaderManager(ShaderManager&&) noexcept = default;
    ShaderManager& operator=(ShaderManager&&) noexcept = default;

    void init_shaders(
        const std::filesystem::path& root_path, 
        const std::vector<std::filesystem::path>* include_directories,
        const std::filesystem::path* debug_root_path
    );
};