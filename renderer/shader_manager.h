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

    VulkanShaderModule test_cs;

    VulkanShaderModule point_vs;
    VulkanShaderModule point_fs;

    VulkanShaderModule gicp_step_cs;
    VulkanShaderModule insert_points_into_voxel_map_cs;
    VulkanShaderModule reset_point_voxel_map_cs;
    VulkanShaderModule gicp_reduce_cs;
};