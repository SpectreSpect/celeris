#include "compute_pass_manager.h"

#include "../vulkan_self/compute/compute_pass_builder.h"
#include "../vulkan_self/vulkan_device.h"
#include "../vulkan_self/vulkan_shader_module.h"
#include "shader_manager.h"

ComputePassManager::ComputePassManager(VulkanDevice& device, ShaderManager& shader_manager)
    :   test_compute_pass(create_test_compute_pass(device, shader_manager.test_cs)),
        gicp_cp(create_gicp_compute_pass(device, shader_manager.gicp_step_cs)),
        point_voxel_map_insert_cp(create_point_voxel_map_insert_compute_pass(device, shader_manager.insert_points_into_voxel_map_cs)),
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

ComputePass ComputePassManager::create_gicp_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;
    builder.add_uniform_buffer(0, ShaderStages::compute); // UniformBufferObject
    builder.add_storage_buffer(1, ShaderStages::compute); // SourcePointBuffer
    builder.add_storage_buffer(2, ShaderStages::compute); // SourceNormalBuffer
    builder.add_storage_buffer(3, ShaderStages::compute); // MapPointCountBuffer
    builder.add_storage_buffer(4, ShaderStages::compute); // MapPointBuffer
    builder.add_storage_buffer(5, ShaderStages::compute); // MapNormalBuffer
    builder.add_storage_buffer(6, ShaderStages::compute); // OutputStorageBuffer
    builder.add_storage_buffer(7, ShaderStages::compute); // VoxelHashTableBuffer
    builder.add_storage_buffer(8, ShaderStages::compute); // OutputPartialBuffer
    builder.add_storage_buffer(9, ShaderStages::compute); // RejectionBuffer
    
    builder.set_compute_shader(compute_shader_module);

    return create_pass(device, builder);
}

ComputePass ComputePassManager::create_point_voxel_map_insert_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;

    builder.add_uniform_buffer(0, ShaderStages::compute);
    builder.add_storage_buffer(1, ShaderStages::compute); // SourcePointBuffer
    builder.add_storage_buffer(2, ShaderStages::compute); // SourceNormalBuffer
    builder.add_storage_buffer(3, ShaderStages::compute); // MapPointCountBuffer
    builder.add_storage_buffer(4, ShaderStages::compute); // MapPointBuffer
    builder.add_storage_buffer(5, ShaderStages::compute); // MapNormalBuffer
    builder.add_storage_buffer(6, ShaderStages::compute); // VoxelHashTableBuffer
    
    builder.set_compute_shader(compute_shader_module);

    return create_pass(device, builder);
}