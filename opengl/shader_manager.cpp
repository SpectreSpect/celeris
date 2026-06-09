#include "shader_manager.h"

ShaderManager::ShaderManager(
    const std::filesystem::path& root_path, 
    const std::vector<std::filesystem::path>* include_directories,
    const std::filesystem::path* debug_root_path) 
{
    init_shaders(root_path, include_directories, debug_root_path);
}

void ShaderManager::init_shaders(
    const std::filesystem::path& root_path, 
    const std::vector<std::filesystem::path>* include_directories,
    const std::filesystem::path* debug_root_path)
{
    if (debug_root_path != nullptr) {
        try {
        std::filesystem::remove_all(*debug_root_path);
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Filesystem error: " << e.what() << '\n';
        }
    }

    const fs::path& p = root_path;
    const fs::path*& d = debug_root_path;
    const fs::path s;

    // general
    dispatch_adapter_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "dispatch_adapter.glsl", p, d, include_directories);
    clear_buffer_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "clear_buffer.glsl", p, d, include_directories);
    clear_buffer_prog = ComputeProgram(&clear_buffer_cs);

    // voxelizaton
    reset_voxelize_pipeline_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_rasterization" / "reset_voxelize_pipeline.glsl", p, d, include_directories);
    mark_and_count_active_chunks_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_rasterization" / "mark_and_count_active_chunks.glsl", p, d, include_directories);
    alloc_active_chunk_triangles_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_rasterization" / "alloc_active_chunk_triangles.glsl", p, d, include_directories);
    fill_triangle_indices_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_rasterization" / "fill_triangle_indices.glsl", p, d, include_directories);
    voxelize_triangles_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_rasterization" / "voxelize_triangles.glsl", p, d, include_directories);

<<<<<<< HEAD:shader_manager.cpp
    // voxel_grid
    clear_chunks_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "clear_chunks.glsl", p, d, include_directories);
    world_init_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "world_init.glsl", p, d, include_directories);
    apply_writes_to_world_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "apply_writes_to_world.glsl", p, d, include_directories);
    mesh_reset_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "mesh_reset.glsl", p, d, include_directories);
    mesh_count_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "mesh_count.glsl", p, d, include_directories);
    mesh_alloc_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "mesh_alloc.glsl", p, d, include_directories);
    mesh_emit_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "mesh_emit.glsl", p, d, include_directories);
    mesh_finalize_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "mesh_finalize.glsl", p, d, include_directories);
    build_indirect_cmds_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "build_indirect_cmds.glsl", p, d, include_directories);
    reset_dirty_count_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "reset_dirty_count.glsl", p, d, include_directories);
    evict_buckets_build_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "evict_buckets_build.glsl", p, d, include_directories);
    evict_low_priority_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "evict_low_priority.glsl", p, d, include_directories);
    evict_low_priority_dispatch_adapter_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "evict_low_priority_dispatch_adapter.glsl", p, d, include_directories);
    stream_select_chunks_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "stream_select_chunks.glsl", p, d, include_directories);
    stream_generate_terrain_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "stream_generate_terrain.glsl", p, d, include_directories);
    mark_all_user_chunks_as_dirty_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "mark_all_user_chunks_as_dirty.glsl", p, d, include_directories);
    mesh_pool_clear_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "mesh_pool_clear.glsl", p, d, include_directories);
    mesh_pool_seed_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "mesh_pool_seed.glsl", p, d, include_directories);
    verify_mesh_allocation_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "verify_mesh_allocation.glsl", p, d, include_directories);
    return_free_alloc_nodes_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "return_free_alloc_nodes.glsl", p, d, include_directories);
    return_free_alloc_nodes_dispatch_adapter_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "return_free_alloc_nodes_dispatch_adapter.glsl", p, d, include_directories);
    free_evicted_chunks_mesh_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "free_evicted_chunks_mesh.glsl", p, d, include_directories);
    fill_chunk_hash_table_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "fill_chunk_hash_table.glsl", p, d, include_directories);
    clear_chunk_hash_table_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "clear_chunk_hash_table.glsl", p, d, include_directories);
    reset_evicted_list_and_buckets_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "reset_evicted_list_and_buckets.glsl", p, d, include_directories);
    hash_table_conditional_dispatch_adapter_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "hash_table_conditional_dispatch_adapter.glsl", p, d, include_directories);
    mark_write_chunks_to_generate_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "mark_write_chunks_to_generate.glsl", p, d, include_directories);
    write_voxels_to_grid_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "write_voxels_to_grid.glsl", p, d, include_directories);
    insert_elements_to_voxel_write_list_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "insert_elements_to_voxel_write_list.glsl", p, d, include_directories);
    add_voxel_write_list_counters_together_cs = Shader::build_shader<ComputeShader>(s / "shaders" / "voxel_grid" / "add_voxel_write_list_counters_together.glsl", p, d, include_directories);
    voxel_mesh_vs = Shader::build_shader<VertexShader>(s / "shaders" / "voxel_grid" / "voxel_mesh.vert", p, d, include_directories);
    voxel_mesh_fs = Shader::build_shader<FragmentShader>(s / "shaders" / "voxel_grid" / "voxel_mesh.frag", p, d, include_directories);
=======
    clear_chunks_cs = ComputeShader(p / "shaders" / "voxel_grid" / "clear_chunks.glsl", include_directories);
    world_init_cs = ComputeShader(p / "shaders" / "voxel_grid" / "world_init.glsl", include_directories);
    apply_writes_to_world_cs = ComputeShader(p / "shaders" / "voxel_grid" / "apply_writes_to_world.glsl", include_directories);
    mesh_reset_cs = ComputeShader(p / "shaders" / "voxel_grid" / "mesh_reset.glsl", include_directories);
    mesh_count_cs = ComputeShader(p / "shaders" / "voxel_grid" / "mesh_count.glsl", include_directories);
    mesh_alloc_cs = ComputeShader(p / "shaders" / "voxel_grid" / "mesh_alloc.glsl", include_directories);
    mesh_emit_cs = ComputeShader(p / "shaders" / "voxel_grid" / "mesh_emit.glsl", include_directories);
    mesh_finalize_cs = ComputeShader(p / "shaders" / "voxel_grid" / "mesh_finalize.glsl", include_directories);
    build_indirect_cmds_cs = ComputeShader(p / "shaders" / "voxel_grid" / "build_indirect_cmds.glsl", include_directories);
    reset_dirty_count_cs = ComputeShader(p / "shaders" / "voxel_grid" / "reset_dirty_count.glsl", include_directories);
    evict_buckets_build_cs = ComputeShader(p / "shaders" / "voxel_grid" / "evict_buckets_build.glsl", include_directories);
    evict_low_priority_cs = ComputeShader(p / "shaders" / "voxel_grid" / "evict_low_priority.glsl", include_directories);
    evict_low_priority_dispatch_adapter_cs = ComputeShader(p / "shaders" / "voxel_grid" / "evict_low_priority_dispatch_adapter.glsl", include_directories);
    stream_select_chunks_cs = ComputeShader(p / "shaders" / "voxel_grid" / "stream_select_chunks.glsl", include_directories);
    stream_generate_terrain_cs = ComputeShader(p / "shaders" / "voxel_grid" / "stream_generate_terrain.glsl", include_directories);
    mark_all_user_chunks_as_dirty_cs = ComputeShader(p / "shaders" / "voxel_grid" / "mark_all_user_chunks_as_dirty.glsl", include_directories);
    mesh_pool_clear_cs = ComputeShader(p / "shaders" / "voxel_grid" / "mesh_pool_clear.glsl", include_directories);
    mesh_pool_seed_cs = ComputeShader(p / "shaders" / "voxel_grid" / "mesh_pool_seed.glsl", include_directories);
    verify_mesh_allocation_cs = ComputeShader(p / "shaders" / "voxel_grid" / "verify_mesh_allocation.glsl", include_directories);
    return_free_alloc_nodes_cs = ComputeShader(p / "shaders" / "voxel_grid" / "return_free_alloc_nodes.glsl", include_directories);
    return_free_alloc_nodes_dispatch_adapter_cs = ComputeShader(p / "shaders" / "voxel_grid" / "return_free_alloc_nodes_dispatch_adapter.glsl", include_directories);
    free_evicted_chunks_mesh_cs = ComputeShader(p / "shaders" / "voxel_grid" / "free_evicted_chunks_mesh.glsl", include_directories);
    fill_chunk_hash_table_cs = ComputeShader(p / "shaders" / "voxel_grid" / "fill_chunk_hash_table.glsl", include_directories);
    clear_chunk_hash_table_cs = ComputeShader(p / "shaders" / "voxel_grid" / "clear_chunk_hash_table.glsl", include_directories);
    reset_evicted_list_and_buckets_cs = ComputeShader(p / "shaders" / "voxel_grid" / "reset_evicted_list_and_buckets.glsl", include_directories);
    hash_table_conditional_dispatch_adapter_cs = ComputeShader(p / "shaders" / "voxel_grid" / "hash_table_conditional_dispatch_adapter.glsl", include_directories);
    voxel_mesh_vs = VertexShader(p / "shaders" / "voxel_grid" / "voxel_mesh.vert", include_directories);
    voxel_mesh_fs = FragmentShader(p / "shaders" / "voxel_grid" / "voxel_mesh.frag", include_directories);

    default_vertex_shader = VertexShader(p / "shaders" / "deafult_vertex.glsl", include_directories);
    default_fragment_shader = FragmentShader(p / "shaders" / "deafult_fragment.glsl", include_directories);

    default_line_vertex_shader = VertexShader(p / "shaders" / "line_vs.glsl", include_directories);
    default_line_fragment_shader = FragmentShader(p / "shaders" / "line_fs.glsl", include_directories);

    default_point_vertex_shader = VertexShader(p / "shaders" / "point_vs.glsl", include_directories);
    default_point_fragment_shader = FragmentShader(p / "shaders" / "point_fs.glsl", include_directories);

    default_cirlce_vertex_shader = VertexShader(p / "shaders" / "circle_vs.glsl", include_directories);

    skybox_vs = VertexShader(p / "shaders" / "skybox_vs.glsl", include_directories);
    skybox_fs = FragmentShader(p / "shaders" / "skybox_fs.glsl", include_directories);

    equirect_to_cubemap_vs = VertexShader(p / "shaders" / "equirect_to_cubemap_vs.glsl", include_directories);
    equirect_to_cubemap_fs = FragmentShader(p / "shaders" / "equirect_to_cubemap_fs.glsl", include_directories);

    irradiance_vs = VertexShader(p / "shaders" / "pbr" / "irradiance_vs.glsl", include_directories);
    irradiance_fs = FragmentShader(p / "shaders" / "pbr" / "irradiance_fs.glsl", include_directories);

    prefilter_vs = VertexShader(p / "shaders" / "pbr" / "prefilter_vs.glsl", include_directories);
    prefilter_fs = FragmentShader(p / "shaders" / "pbr" / "prefilter_fs.glsl", include_directories);

    brdf_lut_vs = VertexShader(p / "shaders" / "pbr" / "brdf_vs.glsl", include_directories);
    brdf_lut_fs = FragmentShader(p / "shaders" / "pbr" / "brdf_fs.glsl", include_directories);

    mesh_counters_reset_cs = ComputeShader(p / "shaders" / "voxel_grid" / "mesh_counters_reset.glsl", include_directories);

    light_incides_for_clusters_cs = ComputeShader(
        p / "shaders" / "light_system" / "light_indices_for_clusters_cs.glsl",
        include_directories
    );

    test_hash_table_cs = ComputeShader(p / "shaders" / "test_hash_table.glsl", include_directories, "/home/spectre/Projects/test_open_3d/shaders/test_hash_table_FULL.glsl");

    add_point_cloud_to_map_cs = ComputeShader(p / "shaders" / "add_point_cloud_to_map_cs.glsl", include_directories, "/home/spectre/Projects/test_open_3d/shaders/add_point_cloud_to_map_cs_FULL.glsl");
    align_point_cloud_cs = ComputeShader(p / "shaders" / "align_point_cloud_cs.glsl", include_directories, "/home/spectre/Projects/test_open_3d/shaders/align_point_cloud_cs_FULL.glsl");
>>>>>>> origin/cloud-to-mesh:opengl/shader_manager.cpp
}
