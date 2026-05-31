#include "shader_manager.h"
#include "../path_utils.h"

ShaderManager::ShaderManager(VulkanDevice& device) 
    :   blinn_phong_vs(device, path_utils::executable_dir() / "shaders" / "triangle.vert.spv"),
        blinn_phong_fs(device, path_utils::executable_dir() / "shaders" / "triangle.frag.spv"),

        unlit_vs(device, path_utils::executable_dir() / "shaders" / "unlit.vert.spv"),
        unlit_fs(device, path_utils::executable_dir() / "shaders" / "unlit.frag.spv"),

        point_vs(device, path_utils::executable_dir() / "shaders" / "point_cloud.vert.spv"),
        point_fs(device, path_utils::executable_dir() / "shaders" / "point_cloud.frag.spv"), 

        // Compute shaders
        // General
        fill_buffer_cs(device, path_utils::executable_dir() / "shaders" / "fill_buffer.comp.spv"),

        // GICP
        gicp_step_cs(device, path_utils::executable_dir() / "shaders" / "gicp_step.comp.spv"),
        insert_points_into_voxel_map_cs(device, path_utils::executable_dir() / "shaders" / "insert_points_into_voxel_map.comp.spv"),
        reset_point_voxel_map_cs(device, path_utils::executable_dir() / "shaders" / "reset_voxel_point_map.comp.spv"),
        gicp_reduce_cs(device, path_utils::executable_dir() / "shaders" / "gicp_reduce.comp.spv"),

        // Lights
        build_cluster_light_lists_cs(device, path_utils::executable_dir() / "shaders" / "build_cluster_light_lists.comp.spv") 
        
        // Voxel grid
        // ...
        {}
