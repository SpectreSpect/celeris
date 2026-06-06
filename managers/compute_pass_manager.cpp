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
        apply_writes_to_world_cp(create_apply_writes_to_world_compute_pass(device, shader_manager.apply_writes_to_world_cs)),
        mesh_pool_clear_cp(create_mesh_pool_clear_compute_pass(device, shader_manager.mesh_pool_clear_cs)),
        mesh_pool_seed_cp(create_mesh_pool_seed_compute_pass(device, shader_manager.mesh_pool_seed_cs)),
        dispatch_adapter_cp(create_dispatch_adapter_compute_pass(device, shader_manager.dispatch_adapter_cs)),
        mesh_reset_cp(create_mesh_reset_compute_pass(device, shader_manager.mesh_reset_cs)),
        mesh_count_cp(create_mesh_count_compute_pass(device, shader_manager.mesh_count_cs)),
        mesh_alloc_cp(create_mesh_alloc_compute_pass(device, shader_manager.mesh_alloc_cs)),
        verify_mesh_allocation_cp(create_verify_mesh_allocation_compute_pass(device, shader_manager.verify_mesh_allocation_cs)),
        stream_select_chunks_cp(create_stream_select_chunks_compute_pass(device, shader_manager.stream_select_chunks_cs)),

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

ComputePass ComputePassManager::create_dispatch_adapter_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;

    builder.set_descriptor_set_flags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

    builder.add_storage_buffer(0, ShaderStages::compute); // Buffer0
    builder.add_storage_buffer(1, ShaderStages::compute); // Buffer1
    builder.add_storage_buffer(2, ShaderStages::compute); // Buffer2
    builder.add_storage_buffer(3, ShaderStages::compute); // DispatchBuf

    builder.add_push_constantsf(sizeof(DispatchAdapterPushConstants), ShaderStages::compute);

    return create_pass(device, compute_shader_module, builder);
}

ComputePass ComputePassManager::create_mesh_reset_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;

    // builder.set_descriptor_set_flags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

    builder.add_storage_buffer(0, ShaderStages::compute); // DirtyListBuf
    builder.add_storage_buffer(1, ShaderStages::compute); // DirtyQuadCountBuf
    builder.add_storage_buffer(2, ShaderStages::compute); // EmitCounterBuf

    return create_pass(device, compute_shader_module, builder);
}

ComputePass ComputePassManager::create_mesh_count_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;

    builder.add_storage_buffer(0, ShaderStages::compute); // ChunkHashTable
    builder.add_storage_buffer(1, ShaderStages::compute); // ChunkVoxels
    builder.add_storage_buffer(2, ShaderStages::compute); // DirtyListBuf
    builder.add_storage_buffer(3, ShaderStages::compute); // DirtyQuadCountBuf
    builder.add_storage_buffer(4, ShaderStages::compute); // ChunkMetaBuf

    builder.add_push_constantsf(sizeof(MeshCountPushConstants), ShaderStages::compute);

    return create_pass(device, compute_shader_module, builder);
}

ComputePass ComputePassManager::create_mesh_alloc_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;

    builder.add_storage_buffer(0, ShaderStages::compute); // MeshBuffersStatusBuf
    builder.add_storage_buffer(1, ShaderStages::compute); // DirtyListBuf
    builder.add_storage_buffer(2, ShaderStages::compute); // DirtyQuadCountBuf
    builder.add_storage_buffer(3, ShaderStages::compute); // ChunkMetaBuf
    builder.add_storage_buffer(4, ShaderStages::compute); // ChunkMeshAllocLocalBuf
    builder.add_storage_buffer(5, ShaderStages::compute); // ChunkMeshAllocGlobalBuf
    builder.add_storage_buffer(6, ShaderStages::compute); // BBHeads
    builder.add_storage_buffer(7, ShaderStages::compute); // BBState
    builder.add_storage_buffer(8, ShaderStages::compute); // BBNodes
    builder.add_storage_buffer(9, ShaderStages::compute); // BBFreeNodesList
    builder.add_storage_buffer(10, ShaderStages::compute); // BBReturnedNodesList

    builder.add_push_constantsf(sizeof(MeshAllocPushConstants), ShaderStages::compute);

    return create_pass(device, compute_shader_module, builder);
}

ComputePass ComputePassManager::create_verify_mesh_allocation_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;

    builder.add_storage_buffer(0, ShaderStages::compute); // LocalChunkMeshAllocBuf
    builder.add_storage_buffer(1, ShaderStages::compute); // GlobalChunkMeshAllocBuf
    builder.add_storage_buffer(2, ShaderStages::compute); // DirtyListBuf
    builder.add_storage_buffer(3, ShaderStages::compute); // MeshBuffersStatusBuf
    builder.add_storage_buffer(4, ShaderStages::compute); // VBHeads
    builder.add_storage_buffer(5, ShaderStages::compute); // VBState
    builder.add_storage_buffer(6, ShaderStages::compute); // VBNodes
    builder.add_storage_buffer(7, ShaderStages::compute); // VBFreeNodesList
    builder.add_storage_buffer(8, ShaderStages::compute); // VBReturnedNodesList
    builder.add_storage_buffer(9, ShaderStages::compute); // IBHeads
    builder.add_storage_buffer(10, ShaderStages::compute); // IBState
    builder.add_storage_buffer(11, ShaderStages::compute); // IBNodes
    builder.add_storage_buffer(12, ShaderStages::compute); // IBFreeNodesList
    builder.add_storage_buffer(13, ShaderStages::compute); // IBReturnedNodesList

    builder.add_push_constantsf(sizeof(VerifyMeshAllocationPushConstants), ShaderStages::compute);

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

ComputePass ComputePassManager::create_apply_writes_to_world_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;

    builder.add_storage_buffer(0, ShaderStages::compute); // ChunkHashTable
    builder.add_storage_buffer(1, ShaderStages::compute); // VoxelWriteList
    builder.add_storage_buffer(2, ShaderStages::compute); // ChunkVoxels
    builder.add_storage_buffer(3, ShaderStages::compute); // FreeList
    builder.add_storage_buffer(4, ShaderStages::compute); // ChunkMetaBuf
    builder.add_storage_buffer(5, ShaderStages::compute); // EnqueuedBuf
    builder.add_storage_buffer(6, ShaderStages::compute); // DirtyListBuf
    builder.add_push_constantsf(sizeof(ApplyVoxelWritesPushConstants), ShaderStages::compute);

    return create_pass(device, compute_shader_module, builder);
}

ComputePass ComputePassManager::create_mesh_pool_clear_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;

    // layout(std430, set = 0, binding=0) buffer VBHeads { uint vb_heads[]; };
    // layout(std430, set = 0, binding=1) buffer VBState { uint vb_state[]; };
    // layout(std430, set = 0, binding=2) buffer VBFreeNodesList  { uint vb_free_nodes_counter; uint vb_free_nodes_list[];  };
    // layout(std430, set = 0, binding=3) buffer IBHeads { uint ib_heads[]; };
    // layout(std430, set = 0, binding=4) buffer IBState { uint ib_state[]; };
    // layout(std430, set = 0, binding=5) buffer IBFreeNodesList  { uint ib_free_nodes_counter; uint ib_free_nodes_list[];  };
    // layout(std430, set = 0, binding=6) buffer ChunkMeshAllocBuf { ChunkMeshAlloc chunk_alloc[]; };

    // layout(std430, set = 0, binding=7) buffer UniformBuffer

    builder.add_storage_buffer(0, ShaderStages::compute); // VBHeads
    builder.add_storage_buffer(1, ShaderStages::compute); // VBState
    builder.add_storage_buffer(2, ShaderStages::compute); // VBFreeNodesList
    builder.add_storage_buffer(3, ShaderStages::compute); // IBHeads
    builder.add_storage_buffer(4, ShaderStages::compute); // IBState
    builder.add_storage_buffer(5, ShaderStages::compute); // IBFreeNodesList
    builder.add_storage_buffer(6, ShaderStages::compute); // ChunkMeshAllocBuf

    builder.add_uniform_buffer(7, ShaderStages::compute); // UniformBuffer
    // builder.add_push_constantsf(sizeof(ApplyVoxelWritesPushConstants), ShaderStages::compute);

    return create_pass(device, compute_shader_module, builder);
}

ComputePass ComputePassManager::create_mesh_pool_seed_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;

    // layout(std430, binding=0) buffer VBHeads { uint vb_heads[]; };
    // layout(std430, binding=1) buffer VBNodes  { Node vb_nodes[];  };
    // layout(std430, binding=2) buffer VBState { uint vb_state[]; };
    // layout(std430, binding=3) buffer VBFreeNodesList  { uint vb_free_nodes_counter; uint vb_free_nodes_list[];  };
    // layout(std430, binding=4) buffer IBHeads { uint ib_heads[]; };
    // layout(std430, binding=5) buffer IBNodes  { Node ib_nodes[];  };
    // layout(std430, binding=6) buffer IBState { uint ib_state[]; };
    // layout(std430, binding=7) buffer IBFreeNodesList  { uint ib_free_nodes_counter; uint ib_free_nodes_list[];  };
    

    builder.add_storage_buffer(0, ShaderStages::compute); // VBHeads
    builder.add_storage_buffer(1, ShaderStages::compute); // VBNodes
    builder.add_storage_buffer(2, ShaderStages::compute); // VBState
    builder.add_storage_buffer(3, ShaderStages::compute); // VBFreeNodesList
    builder.add_storage_buffer(4, ShaderStages::compute); // IBHeads
    builder.add_storage_buffer(5, ShaderStages::compute); // IBNodes
    builder.add_storage_buffer(6, ShaderStages::compute); // IBState
    builder.add_storage_buffer(7, ShaderStages::compute); // IBFreeNodesList

    builder.add_uniform_buffer(8, ShaderStages::compute); // UniformBuffer

    // builder.add_push_constantsf(sizeof(ApplyVoxelWritesPushConstants), ShaderStages::compute);

    return create_pass(device, compute_shader_module, builder);
}

ComputePass ComputePassManager::create_stream_select_chunks_compute_pass(VulkanDevice& device, VulkanShaderModule& compute_shader_module) {
    LOG_METHOD();

    ComputePassBuilder builder;

    builder.add_storage_buffer(0, ShaderStages::compute); // ChunkHashTable
    builder.add_storage_buffer(1, ShaderStages::compute); // FreeList
    builder.add_storage_buffer(2, ShaderStages::compute); // ChunkMetaBuf
    builder.add_storage_buffer(3, ShaderStages::compute); // EnqueuedBuf
    builder.add_storage_buffer(4, ShaderStages::compute); // LoadList
    
    builder.add_push_constantsf(sizeof(StreamSelectChunksPushConstants), ShaderStages::compute);

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
