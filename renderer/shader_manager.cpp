#include "shader_manager.h"
#include "../path_utils.h"

ShaderManager::ShaderManager(VulkanDevice& device) 
    :   blinn_phong_vs(device, path_utils::executable_dir() / "shaders" / "triangle.vert.spv"),
        blinn_phong_fs(device, path_utils::executable_dir() / "shaders" / "triangle.frag.spv"),

        unlit_vs(device, path_utils::executable_dir() / "shaders" / "unlit.vert.spv"),
        unlit_fs(device, path_utils::executable_dir() / "shaders" / "unlit.frag.spv"),

        test_cs(device, path_utils::executable_dir() / "shaders" / "test_compute_shader.comp.spv"),
        point_vs(device, path_utils::executable_dir() / "shaders" / "point_cloud.vert.spv"),
        point_fs(device, path_utils::executable_dir() / "shaders" / "point_cloud.frag.spv") {}
