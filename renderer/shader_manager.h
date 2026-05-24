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
};