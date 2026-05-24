#pragma once

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/compute/compute_pass.h"
#include "../vulkan_self/descriptor_set/descriptor_pool.h"
#include "../vulkan_self/descriptor_set/descriptor_pool_builder.h"

class VulkanDevice;
class VulkanShaderModule;
class ShaderManager;

class ComputePassManager {
public:
    _XCLASS_NAME(ComputePassManager);

private:
    DescriptorPoolBuilder m_pool_builder;

public:
    ComputePass test_compute_pass;

    ComputePassManager(VulkanDevice& device, ShaderManager& shader_manager);

    DescriptorPool& descriptor_pool() noexcept;

    ComputePass create_pass(VulkanDevice& device, ComputePassBuilder& builder);

    ComputePass create_test_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module);

private:
    
    DescriptorPool m_pool;
    static constexpr uint32_t m_max_compute_pass_instances = 256;
};
