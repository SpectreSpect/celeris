#include "compute_pass_manager.h"

#include "../vulkan_self/compute/compute_pass_builder.h"
#include "../vulkan_self/vulkan_device.h"
#include "../vulkan_self/vulkan_shader_module.h"
#include "shader_manager.h"

ComputePassManager::ComputePassManager(VulkanDevice& device, ShaderManager& shader_manager)
    :   test_compute_pass(create_test_compute_pass(device, shader_manager.test_cs)),
        m_pool(device, m_pool_builder) {}

DescriptorPool& ComputePassManager::descriptor_pool() noexcept {
    return m_pool;
}

ComputePass ComputePassManager::create_pass(VulkanDevice& device, ComputePassBuilder& builder) {
    LOG_METHOD();
    m_pool_builder.add_layout(builder.material_dsl_builder(), m_max_compute_pass_instances);
    return ComputePass(device, builder);
}

ComputePass ComputePassManager::create_test_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;
    builder.add_uniform_buffer(0, ShaderStages::compute);
    builder.add_storage_buffer(1, ShaderStages::compute);
    builder.set_compute_shader(compute_shader_module);

    return create_pass(device, builder);
}
