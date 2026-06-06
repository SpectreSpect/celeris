#include "shader_manager.h"
#include "../path_utils.h"

ShaderManager::ShaderManager(VulkanDevice& device) 
    :   blinn_phong_vs(device, path_utils::executable_dir() / "shaders" / "triangle.vert.spv"),
        blinn_phong_fs(device, path_utils::executable_dir() / "shaders" / "triangle.frag.spv"),

        unlit_vs(device, path_utils::executable_dir() / "shaders" / "unlit.vert.spv"),
        unlit_fs(device, path_utils::executable_dir() / "shaders" / "unlit.frag.spv"),

        point_vs(device, path_utils::executable_dir() / "shaders" / "point_cloud.vert.spv"),
        point_fs(device, path_utils::executable_dir() / "shaders" / "point_cloud.frag.spv"),

        skybox_vs(device, path_utils::executable_dir() / "shaders" / "skybox.vert.spv"),
        skybox_fs(device, path_utils::executable_dir() / "shaders" / "skybox.frag.spv"),
        pbr_vs(device, path_utils::executable_dir() / "shaders" / "pbr.vert.spv"),
        pbr_fs(device, path_utils::executable_dir() / "shaders" / "pbr.frag.spv"),

        // Compute shaders
        // General
        fill_buffer_cs(device, path_utils::executable_dir() / "shaders" / "fill_buffer.comp.spv"),

        // GICP
        gicp_step_cs(device, path_utils::executable_dir() / "shaders" / "gicp_step.comp.spv"),
        insert_points_into_voxel_map_cs(device, path_utils::executable_dir() / "shaders" / "insert_points_into_voxel_map.comp.spv"),
        reset_point_voxel_map_cs(device, path_utils::executable_dir() / "shaders" / "reset_voxel_point_map.comp.spv"),
        gicp_reduce_cs(device, path_utils::executable_dir() / "shaders" / "gicp_reduce.comp.spv"),

        // Lights
        build_cluster_light_lists_cs(device, path_utils::executable_dir() / "shaders" / "build_cluster_light_lists.comp.spv"),
        
        // Voxel grid
        world_init_cs(device, path_utils::executable_dir() / "shaders" / "voxel_grid" / "world_init.comp.spv"),
        apply_writes_to_world_cs(device, path_utils::executable_dir() / "shaders" / "voxel_grid" / "apply_writes_to_world.comp.spv"),
        mesh_pool_clear_cs(device, path_utils::executable_dir() / "shaders" / "voxel_grid" / "mesh_pool_clear.comp.spv"),
        mesh_pool_seed_cs(device, path_utils::executable_dir() / "shaders" / "voxel_grid" / "mesh_pool_seed.comp.spv"),
        dispatch_adapter_cs(device, path_utils::executable_dir() / "shaders" / "dispatch_adapter.comp.spv"),
        mesh_reset_cs(device, path_utils::executable_dir() / "shaders" / "voxel_grid" / "mesh_reset.comp.spv"),
        mesh_count_cs(device, path_utils::executable_dir() / "shaders" / "voxel_grid" / "mesh_count.comp.spv"),
        mesh_alloc_cs(device, path_utils::executable_dir() / "shaders" / "voxel_grid" / "mesh_alloc.comp.spv"),
        stream_select_chunks_cs(device, path_utils::executable_dir() / "shaders" / "voxel_grid" / "stream_select_chunks.comp.spv"),

        // PBR
        equirect_to_cubemap_cs(device, path_utils::executable_dir() / "shaders" / "equirect_to_cubemap.comp.spv"),
        brdf_lut_cs(device, path_utils::executable_dir() / "shaders" / "brdf_lut.comp.spv"),
        generate_prefilter_map_cs(device, path_utils::executable_dir() / "shaders" / "generate_prefilter_map.comp.spv"),
        generate_irradiance_map_cs(device, path_utils::executable_dir() / "shaders" / "generate_irradiance_map.comp.spv") {}
