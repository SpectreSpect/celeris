#include "shader_manager.h"
#include "../path_utils.h"

ShaderManager::ShaderManager(VulkanDevice& device) 
    :   blinn_phong_vs(device, path_utils::executable_dir() / "shaders" / "triangle.vert.spv"),
        blinn_phong_fs(device, path_utils::executable_dir() / "shaders" / "triangle.frag.spv"),

        unlit_vs(device, path_utils::executable_dir() / "shaders" / "unlit.vert.spv"),
        unlit_fs(device, path_utils::executable_dir() / "shaders" / "unlit.frag.spv"),

        test_cs(device, path_utils::executable_dir() / "shaders" / "test_compute_shader.comp.spv"),
        point_vs(device, path_utils::executable_dir() / "shaders" / "point_cloud.vert.spv"),
        point_fs(device, path_utils::executable_dir() / "shaders" / "point_cloud.frag.spv"), 
        skybox_vs(device, path_utils::executable_dir() / "shaders" / "skybox.vert.spv"),
        skybox_fs(device, path_utils::executable_dir() / "shaders" / "skybox.frag.spv"),
        gicp_step_cs(device, path_utils::executable_dir() / "shaders" / "gicp_step.comp.spv"),
        insert_points_into_voxel_map_cs(device, path_utils::executable_dir() / "shaders" / "insert_points_into_voxel_map.comp.spv"),
        reset_point_voxel_map_cs(device, path_utils::executable_dir() / "shaders" / "reset_voxel_point_map.comp.spv"),
        gicp_reduce_cs(device, path_utils::executable_dir() / "shaders" / "gicp_reduce.comp.spv"),
        build_cluster_light_lists_cs(device, path_utils::executable_dir() / "shaders" / "build_cluster_light_lists.comp.spv"),
        equirect_to_cubemap_cs(device, path_utils::executable_dir() / "shaders" / "equirect_to_cubemap.comp.spv") {}
