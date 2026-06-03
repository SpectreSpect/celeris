#include "compute_pass_manager.h"

#include "../vulkan_self/pass/compute_pass/compute_pass_builder.h"
#include "../vulkan_self/vulkan_device.h"
#include "../vulkan_self/vulkan_shader_module.h"
#include "shader_manager.h"

#include "../vulkan_self/push_constants_structures.h"

ComputePassManager::ComputePassManager(VulkanDevice& device, ShaderManager& shader_manager)
    :   
        // General
        fill_buffer_cp(create_fill_buffer_compute_pass(device, shader_manager.fill_buffer_cs)),

        // GICP
        gicp_cp(create_gicp_compute_pass(device, shader_manager.gicp_step_cs)),
        point_voxel_map_insert_cp(create_point_voxel_map_insert_compute_pass(device, shader_manager.insert_points_into_voxel_map_cs)),
        reset_voxel_point_map_cp(create_reset_voxel_point_map_compute_pass(device, shader_manager.reset_point_voxel_map_cs)),
        gicp_reduce_cp(create_gicp_reduce_compute_pass(device, shader_manager.gicp_reduce_cs)),

        // Lights
        build_cluster_light_lists_cp(create_build_cluster_light_lists_compute_pass(device, shader_manager.build_cluster_light_lists_cs)),

        // Voxel grid
        world_init_cp(create_world_init_compute_pass(device, shader_manager.world_init_cs)),

        // PBR
        equirect_to_cubemap_cp(create_equirect_to_cubemap_compute_pass(device, shader_manager.equirect_to_cubemap_cs)),
        brdf_lut_cp(create_brdf_lut_pass(device, shader_manager.brdf_lut_cs)),
        prefilter_map_cp(create_prefilter_map_pass(device, shader_manager.generate_prefilter_map_cs)),
        irradiance_map_cp(create_irradiance_map_pass(device, shader_manager.generate_irradiance_map_cs)),
        m_pool(device, m_pool_builder) {}

DescriptorPool& ComputePassManager::descriptor_pool() noexcept {
    return m_pool;
}

ComputePass ComputePassManager::create_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module, ComputePassBuilder& builder) {
    LOG_METHOD();
    
    builder.set_compute_shader(compute_shader_module);
    m_pool_builder.add_layout(builder.material_dsl_builder(), m_max_compute_pass_instances);

    return ComputePass(device, builder);
}

ComputePass ComputePassManager::create_fill_buffer_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;
    builder.add_storage_buffer(0, ShaderStages::compute); // PrefabBuffer
    builder.add_storage_buffer(1, ShaderStages::compute); // ClearableBuffer
    builder.add_push_constantsf(sizeof(FillBufferPushConstants), ShaderStages::compute);

    return create_pass(device, compute_shader_module, builder);
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

    return create_pass(device, compute_shader_module, builder);
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

    return create_pass(device, compute_shader_module, builder);
}

ComputePass ComputePassManager::create_reset_voxel_point_map_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;

    builder.add_uniform_buffer(0, ShaderStages::compute);
    // builder.add_storage_buffer(1, ShaderStages::compute); 
    builder.add_storage_buffer(2, ShaderStages::compute); // MapPointCountBuffer
    builder.add_storage_buffer(4, ShaderStages::compute); // VoxelHashTableBuffer

    return create_pass(device, compute_shader_module, builder);
}

ComputePass ComputePassManager::create_gicp_reduce_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;

    builder.add_uniform_buffer(0, ShaderStages::compute); // UniformBufferObject
    builder.add_storage_buffer(1, ShaderStages::compute); // SourcePartialBuffer
    builder.add_storage_buffer(2, ShaderStages::compute); // OutputPartialBuffer

    return create_pass(device, compute_shader_module, builder);
}

ComputePass ComputePassManager::create_build_cluster_light_lists_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;

    builder.add_uniform_buffer(0, ShaderStages::compute); // lighting_system_uniform_buffer
    builder.add_storage_buffer(4, ShaderStages::compute); // cluster_aabbs_ssbo
    builder.add_storage_buffer(5, ShaderStages::compute); // light_source_ssbo
    builder.add_storage_buffer(6, ShaderStages::compute); // num_lights_in_clusters_ssbo
    builder.add_storage_buffer(7, ShaderStages::compute); // lights_in_clusters_ssbo

    return create_pass(device, compute_shader_module, builder);
}

ComputePass ComputePassManager::create_world_init_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;

    builder.add_storage_buffer(0, ShaderStages::compute); // ChunkHashTable
    builder.add_storage_buffer(1, ShaderStages::compute); // FreeList
    builder.add_storage_buffer(2, ShaderStages::compute); // MeshBuffersStatusBuf
    builder.add_storage_buffer(3, ShaderStages::compute); // ChunkMetaBuf
    builder.add_storage_buffer(4, ShaderStages::compute); // EnqueuedBuf
    builder.add_storage_buffer(5, ShaderStages::compute); // DirtyListBuf
    builder.add_storage_buffer(6, ShaderStages::compute); // VoxelWriteList
    builder.add_storage_buffer(7, ShaderStages::compute); // IndirectCmdBuf
    builder.add_storage_buffer(8, ShaderStages::compute); // FailedDirtyListBuf
    builder.add_push_constantsf(sizeof(WorldInitPushConstants), ShaderStages::compute); // WorldInitPushConstants

    return create_pass(device, compute_shader_module, builder);
}

ComputePass ComputePassManager::create_equirect_to_cubemap_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;

    builder.add_uniform_buffer(0, ShaderStages::compute); // UniformBufferObject
    builder.add_combined_image_sampler(1, ShaderStages::compute); // UniformBufferObject
    builder.add_storage_image(2, ShaderStages::compute); // outEnvMap
    
    return create_pass(device, compute_shader_module, builder);
}

ComputePass ComputePassManager::create_brdf_lut_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;

    builder.add_uniform_buffer(0, ShaderStages::compute);
    builder.add_storage_image(1, ShaderStages::compute);
    
    return create_pass(device, compute_shader_module, builder);
}

ComputePass ComputePassManager::create_prefilter_map_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;

    builder.add_uniform_buffer(0, ShaderStages::compute); // UniformBufferObject
    builder.add_combined_image_sampler(1, ShaderStages::compute); // uEnvironmentMap
    builder.add_storage_image(2, ShaderStages::compute); // outPrefilterMap
    
    return create_pass(device, compute_shader_module, builder);
}

ComputePass ComputePassManager::create_irradiance_map_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;

    builder.add_uniform_buffer(0, ShaderStages::compute); // UniformBufferObject
    builder.add_combined_image_sampler(1, ShaderStages::compute); // uEnvironmentMap
    builder.add_storage_image(2, ShaderStages::compute); // outIrradianceMap
    
    return create_pass(device, compute_shader_module, builder);
}
