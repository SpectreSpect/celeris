#include "voxel_grid.h"

#include <string>
#include <algorithm>

#include "../math_utils.h"
#include "../managers/compute_pass_manager.h"
#include "../managers/material_instance_manager.h"
#include "voxel_grid_structures.h"
#include "../vulkan_self/vulkan_physical_device.h"
#include "../vulkan_self/vulkan_device.h"
#include "../vulkan_self/descriptor_set/descriptor_pool.h"
#include "../vulkan_self/vulkan_queue.h"
#include "../vulkan_self/push_constants_structures.h"

VoxelGrid::VoxelGrid(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VulkanQueue& queue,
    ComputePassManager& compute_pass_manager,
    MaterialInstanceManager& material_instance_manager,
    const VoxelGridDesc& desc) 
    :   m_command_pool(device, queue),
        m_command_buffer(device, m_command_pool),
        m_fence(device),
        m_queue(&queue),
        m_compute_pass_manager(&compute_pass_manager),
        // m_buffer_filler(physical_device, device, compute_pass_manager, 1024),
        m_params(create_params(desc)),
        m_pass_instances(create_pass_instances(compute_pass_manager)),
        m_buffers(create_buffers(physical_device, device, m_command_buffer)),
        m_mesh_view(m_buffers.global_vertex_buffer.get_view(), m_buffers.global_index_buffer.get_view(), m_params.max_mesh_indices),
        m_render_object(m_mesh_view, material_instance_manager.pbr)
{
    LOG_METHOD();

    



    // init_programs(*shader_manager); #TODO

    // dispatch_args = BufferObject::from_fill(sizeof(uint32_t) * 3u, GL_DYNAMIC_DRAW, 1u, *shader_manager);
    // dispatch_args_additional = BufferObject::from_fill(sizeof(uint32_t) * 3u, GL_DYNAMIC_DRAW, 1u, *shader_manager);
    
    



    // dirty_quad_count_ = BufferObject(sizeof(uint32_t) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);
    // emit_counters_     = BufferObject(sizeof(uint32_t) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);

    BucketHead bucket_head;
    bucket_head.id = INVALID_ID;
    bucket_head.count = 0;
    
    // bucket_heads_ = BufferObject::from_fill(sizeof(BucketHead) * count_evict_buckets, GL_DYNAMIC_DRAW, bucket_head, *shader_manager);
    // bucket_next_  = BufferObject::from_fill(sizeof(uint32_t) * count_active_chunks, GL_DYNAMIC_DRAW, INVALID_ID, *shader_manager);
    // verify_debug_stack_ = BufferObject::from_fill(sizeof(uint32_t) * 2 + sizeof(DebugStackElement) * 10'000, GL_DYNAMIC_DRAW, INVALID_ID, *shader_manager);
    // verify_debug_stack_.update_subdata_fill(0, 0u, sizeof(uint32_t) * 2, *shader_manager);

    // evicted_chunks_list_ = BufferObject::from_fill(sizeof(uint32_t) * (count_active_chunks + 1), GL_DYNAMIC_DRAW, 0u, *shader_manager);
    
    // local_voxel_write_list_ = BufferObject::from_fill(sizeof(uint32_t) * 4 + sizeof(VoxelWriteGPU) * desc.max_write_count, GL_DYNAMIC_DRAW, 0u, *shader_manager);



    // global_vertex_buffer_ = BufferObject(sizeof(VertexGPU) * (size_t)max_mesh_vertices_, GL_DYNAMIC_DRAW);
    // vb_heads_ = BufferObject(sizeof(uint32_t) * (size_t)(vb_order_ + 1), GL_DYNAMIC_DRAW);
    // vb_nodes_ = BufferObject(sizeof(AllocNode) * (size_t)(count_vb_nodes_), GL_DYNAMIC_DRAW);
    // vb_free_nodes_list_ = BufferObject(sizeof(uint32_t) * (size_t)(1u + count_vb_nodes_), GL_DYNAMIC_DRAW);
    // vb_returned_nodes_list = BufferObject::from_fill(sizeof(uint32_t) * (size_t)(1u + count_vb_nodes_), GL_DYNAMIC_DRAW, 0u, *shader_manager);
    // vb_state_ = BufferObject(sizeof(uint32_t) * (size_t)count_vb_pages_, GL_DYNAMIC_DRAW);
    
    // global_index_buffer_ = BufferObject(sizeof(uint32_t) * (size_t)max_mesh_indices_, GL_DYNAMIC_DRAW);
    // ib_heads_ = BufferObject(sizeof(uint32_t) * (size_t)(ib_order_ + 1), GL_DYNAMIC_DRAW);
    // ib_nodes_ = BufferObject(sizeof(AllocNode) * (size_t)(count_ib_nodes_), GL_DYNAMIC_DRAW);
    // ib_free_nodes_list_ = BufferObject(sizeof(uint32_t) * (size_t)(1u + count_ib_nodes_), GL_DYNAMIC_DRAW);
    // ib_returned_nodes_list = BufferObject::from_fill(sizeof(uint32_t) * (size_t)(1u + count_ib_nodes_), GL_DYNAMIC_DRAW, 0u, *shader_manager);
    // ib_state_ = BufferObject(sizeof(uint32_t) * (size_t)count_ib_pages_, GL_DYNAMIC_DRAW);

    // chunk_mesh_alloc_ = BufferObject(sizeof(ChunkMeshAlloc) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);
    // chunk_mesh_alloc_local_ = BufferObject(sizeof(ChunkMeshAlloc) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);

    // voxel_prifab_ = BufferObject(sizeof(VoxelDataGPU), GL_DYNAMIC_DRAW);

    // load_list_ = BufferObject(sizeof(uint32_t) * (size_t)(1 + count_active_chunks), GL_DYNAMIC_DRAW);

    // voxels_ = BufferObject::from_fill(sizeof(VoxelDataGPU) * vox_per_chunk() * count_active_chunks, GL_DYNAMIC_DRAW, voxel_prifab, *shader_manager);

    // alignof(ChunkHashTableSlot) == 8!!!


    world_init_gpu();
    // init_draw_buffers();
    init_mesh_pool();
}

uint64_t VoxelGrid::vox_per_chunk() const noexcept {
    return static_cast<uint64_t>(m_params.chunk_size.x) * 
           static_cast<uint64_t>(m_params.chunk_size.y) * 
           static_cast<uint64_t>(m_params.chunk_size.z);
}

VoxelGrid::VoxelGridParams VoxelGrid::create_params(const VoxelGridDesc& desc) const {
    LOG_METHOD();

    logger.check(desc.chunk_hash_table_size_factor >= 1.0f, "chunk_hash_table_size_factor must be >= 1.0");

    VoxelGridParams params;

    params.chunk_size = desc.chunk_size;
    params.voxel_size = desc.voxel_size;
    params.count_active_chunks = desc.count_active_chunks;
    params.count_evict_buckets = desc.count_evict_buckets;
    params.max_write_count = desc.max_write_count;
    params.min_free_chunks = desc.min_free_chunks;
    params.tomb_fraction_to_rebuild = desc.tomb_fraction_to_rebuild;
    params.eviction_bucket_shell_thickness = desc.eviction_bucket_shell_thickness;
    params.render_distance = desc.render_distance;
    params.generation_distance = desc.generation_distance;

    uint64_t raw = (uint64_t)std::ceil((double)desc.chunk_hash_table_size_factor * (double)desc.count_active_chunks);
    uint32_t base = (raw > UINT32_MAX) ? UINT32_MAX : (uint32_t)raw;
    params.chunk_hash_table_size = math_utils::next_pow2_u32(base);

    logger.check((params.chunk_hash_table_size & (params.chunk_hash_table_size - 1)) == 0, "chunk_hash_table_size must be 2^n");

    params.vb_page_size = 1 << desc.vb_page_size_order_of_two;
    params.count_vb_pages = math_utils::next_pow2_u32(math_utils::div_up_u32((desc.max_quads * 4u), params.vb_page_size));
    params.count_vb_nodes = ceil(params.count_vb_pages * desc.buddy_allocator_nodes_factor);
    params.vb_order = math_utils::log2_floor_u32(params.count_vb_pages);
    params.max_mesh_vertices = params.count_vb_pages * params.vb_page_size;
    
    logger.log() << clr("m_vb_page_size", LoggerPalette::blue) << ": " << std::to_string(params.vb_page_size) << "\n";
    logger.log() << clr("m_count_vb_pages", LoggerPalette::blue) << ": " << std::to_string(params.count_vb_pages) << "\n";
    logger.log() << clr("m_count_vb_nodes", LoggerPalette::blue) << ": " << std::to_string(params.count_vb_nodes) << "\n";
    logger.log() << clr("m_vb_order", LoggerPalette::blue) << ": " << std::to_string(params.vb_order) << "\n";
    logger.log() << clr("m_max_mesh_vertices", LoggerPalette::blue) << ": " << std::to_string(params.max_mesh_vertices) << "\n";
    logger.log() << "\n";

    params.ib_page_size = 1 << desc.ib_page_size_order_of_two;
    params.count_ib_pages = math_utils::next_pow2_u32(math_utils::div_up_u32((desc.max_quads * 6u), params.ib_page_size));
    params.count_ib_nodes = ceil(params.count_ib_pages * desc.buddy_allocator_nodes_factor);
    params.ib_order = math_utils::log2_floor_u32(params.count_ib_pages);
    params.max_mesh_indices = params.count_ib_pages * params.ib_page_size;

    logger.log() << clr("m_ib_page_size", LoggerPalette::blue) << ": " << std::to_string(params.ib_page_size) << "\n";
    logger.log() << clr("m_count_ib_pages", LoggerPalette::blue) << ": " << std::to_string(params.count_ib_pages) << "\n";
    logger.log() << clr("m_count_ib_nodes", LoggerPalette::blue) << ": " << std::to_string(params.count_ib_nodes) << "\n";
    logger.log() << clr("m_ib_order", LoggerPalette::blue) << ": " << std::to_string(params.ib_order) << "\n";
    logger.log() << clr("m_max_mesh_indices", LoggerPalette::blue) << ": " << std::to_string(params.max_mesh_indices) << "\n";
    logger.log() << "\n";

    return params;
}

VoxelGrid::VoxelGridPassInstances VoxelGrid::create_pass_instances(ComputePassManager& compute_pass_manager) const {
    LOG_METHOD();
    
    DescriptorPool& dp = compute_pass_manager.descriptor_pool();

    return VoxelGridPassInstances {
        .fill_buffer_pi = PassInstance(compute_pass_manager.fill_buffer_cp, dp),
        .world_init_pi = PassInstance(compute_pass_manager.world_init_cp, dp),
        .apply_writes_to_world_pi = PassInstance(compute_pass_manager.apply_writes_to_world_cp, dp),
        .mesh_pool_clear_pi = PassInstance(compute_pass_manager.mesh_pool_clear_cp, dp),
        .mesh_pool_seed_pi = PassInstance(compute_pass_manager.mesh_pool_seed_cp, dp)
    };
}

VoxelGrid::VoxelGridBuffers VoxelGrid::create_buffers(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VulkanCommandBuffer& command_buffer) 
{
    LOG_METHOD();
    
    VkDeviceSize free_list_size = sizeof(uint32_t) * (size_t)(1 + m_params.count_active_chunks);
    VkDeviceSize chunk_hash_table_size = sizeof(HashTableCounters) + sizeof(ChunkHashTableSlot) * m_params.chunk_hash_table_size;
    VkDeviceSize mesh_buffers_status_size = sizeof(uint32_t) * 2;
    VkDeviceSize chunk_meta_size = sizeof(ChunkMetaGPU) * (size_t)m_params.count_active_chunks;
    VkDeviceSize enqueued_size = sizeof(uint32_t) * (size_t)m_params.count_active_chunks;
    VkDeviceSize dirty_list_size = sizeof(uint32_t) * (size_t)(1 + m_params.count_active_chunks);
    VkDeviceSize voxel_write_list_size = sizeof(uint32_t) * 4 + sizeof(VoxelWriteGPU) * m_params.max_write_count;
    VkDeviceSize voxels_size = sizeof(VoxelDataGPU) * vox_per_chunk() * (size_t)m_params.count_active_chunks;
    VkDeviceSize indirect_cmds_size = sizeof(uint32_t) + sizeof(DrawElementsIndirectCommand) * (size_t)m_params.count_active_chunks;
    VkDeviceSize failed_dirty_list_size = sizeof(uint32_t) * (size_t)(1 + m_params.count_active_chunks);
    
    VkDeviceSize global_vertex_buffer_size = sizeof(VertexGPU) * m_params.max_mesh_vertices;
    VkDeviceSize global_index_buffer_size = sizeof(uint32_t) * m_params.max_mesh_indices;

    VkDeviceSize vb_heads_size = sizeof(uint32_t) * (size_t)(m_params.vb_order + 1);
    VkDeviceSize vb_nodes_size = sizeof(AllocNode) * (size_t)(m_params.count_vb_nodes);
    VkDeviceSize vb_state_size = sizeof(uint32_t) * m_params.count_vb_pages;
    VkDeviceSize vb_free_nodes_list_size = sizeof(uint32_t) * (size_t)(1u + m_params.count_vb_nodes);
    
    VkDeviceSize ib_heads_size = sizeof(uint32_t) * (size_t)(m_params.ib_order + 1);
    VkDeviceSize ib_nodes_size = sizeof(AllocNode) * (size_t)(m_params.count_ib_nodes);
    VkDeviceSize ib_state_size = sizeof(uint32_t) * m_params.count_ib_pages;
    VkDeviceSize ib_free_nodes_list_size = sizeof(uint32_t) * (size_t)(1u + m_params.count_ib_nodes);

    VkDeviceSize chunk_mesh_alloc_size = sizeof(ChunkMeshAlloc) * m_params.count_active_chunks;

    
    return VoxelGridBuffers {
        // .chunk_hash_table = VulkanBuffer::create_storage_buffer(physical_device, device, chunk_hash_table_size),
        // .free_list = VulkanBuffer::create_storage_buffer(physical_device, device, free_list_size),
        // .chunk_meta = VulkanBuffer::create_storage_buffer(physical_device, device, chunk_meta_size),
        // .enqueued = VulkanBuffer::create_storage_buffer(physical_device, device, enqueued_size),
        // .indirect_cmds = VulkanBuffer::create_storage_buffer(physical_device, device, indirect_cmds_size),
        // .failed_dirty_list = VulkanBuffer::create_storage_buffer(physical_device, device, failed_dirty_list_size),
        .chunk_hash_table = VulkanBuffer::create_host_visible_storage_buffer(physical_device, device, chunk_hash_table_size),
        .free_list = VulkanBuffer::create_host_visible_storage_buffer(physical_device, device, free_list_size),
        .chunk_meta = VulkanBuffer::create_host_visible_storage_buffer(physical_device, device, chunk_meta_size),
        .enqueued = VulkanBuffer::create_host_visible_storage_buffer(physical_device, device, enqueued_size),
        .indirect_cmds = VulkanBuffer::create_host_visible_storage_buffer(physical_device, device, indirect_cmds_size),
        .failed_dirty_list = VulkanBuffer::create_host_visible_storage_buffer(physical_device, device, failed_dirty_list_size),

        // .mesh_buffers_status = VulkanBuffer::create_storage_buffer(physical_device, device, mesh_buffers_status_size),
        // .dirty_list = VulkanBuffer::create_storage_buffer(physical_device, device, dirty_list_size),
        .mesh_buffers_status = VulkanBuffer::create_host_visible_storage_buffer(physical_device, device, mesh_buffers_status_size),
        .dirty_list = VulkanBuffer::create_host_visible_storage_buffer(physical_device, device, dirty_list_size),

        .voxel_write_list = VulkanBuffer::create_host_visible_storage_buffer(physical_device, device, voxel_write_list_size),
        .voxels = VulkanBuffer(
            physical_device,
            device,
            voxels_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        ),
        .global_vertex_buffer = VulkanBuffer::create_vertex_buffer(physical_device, device, global_vertex_buffer_size),
        .global_index_buffer =  VulkanBuffer::create_index_buffer(physical_device, device, global_index_buffer_size),

        .vb_heads = VulkanBuffer::create_host_visible_storage_buffer(physical_device, device, vb_heads_size),
        // .vb_nodes = BufferObject(sizeof(AllocNode) * (size_t)(count_vb_nodes_), GL_DYNAMIC_DRAW);
        .vb_nodes = VulkanBuffer::create_host_visible_storage_buffer(physical_device, device, vb_nodes_size),
        .vb_state = VulkanBuffer::create_host_visible_storage_buffer(physical_device, device, vb_state_size),
        .vb_free_nodes_list = VulkanBuffer::create_host_visible_storage_buffer(physical_device, device, vb_free_nodes_list_size),

        .ib_heads = VulkanBuffer::create_host_visible_storage_buffer(physical_device, device, ib_heads_size),
        .ib_nodes = VulkanBuffer::create_host_visible_storage_buffer(physical_device, device, ib_nodes_size),
        .ib_state = VulkanBuffer::create_host_visible_storage_buffer(physical_device, device, ib_state_size),
        .ib_free_nodes_list = VulkanBuffer::create_host_visible_storage_buffer(physical_device, device, ib_free_nodes_list_size),

        .chunk_mesh_alloc = VulkanBuffer::create_host_visible_storage_buffer(physical_device, device, chunk_mesh_alloc_size),

        .mesh_pool_clear_uniform = VulkanBuffer::create_host_visible_uniform_buffer(physical_device, device, sizeof(MeshPoolClearUniform)),
        .mesh_pool_seed_uniform = VulkanBuffer::create_host_visible_uniform_buffer(physical_device, device, sizeof(MeshPoolSeedUniform))
        
        


        // global_vertex_buffer_ = BufferObject(sizeof(VertexGPU) * (size_t)max_mesh_vertices_, GL_DYNAMIC_DRAW);
        // global_index_buffer_ = BufferObject(sizeof(uint32_t) * (size_t)max_mesh_indices_, GL_DYNAMIC_DRAW);

        // .mesh_buffers_status = m_buffer_filler.fill_buffer(
        //     command_buffer, 0u, VulkanBuffer::create_storage_buffer(physical_device, device, mesh_buffers_status_size)
        // ),
        // .dirty_list = m_buffer_filler.fill_buffer(
        //     command_buffer, 0u, VulkanBuffer::create_storage_buffer(physical_device, device, dirty_list_size)
        // ),
        // .voxel_write_list = m_buffer_filler.fill_buffer(
        //     command_buffer, 0u, VulkanBuffer::create_storage_buffer(physical_device, device, voxel_write_list_size)
        // )
    };
}

void VoxelGrid::world_init_gpu() {
    LOG_METHOD();

    {
        auto scope = m_command_buffer.begin_scope();

        m_buffers.voxels.fill(m_command_buffer, 0u);
        m_buffers.voxels.transfer_write_to_compute_read_write_barrier(m_command_buffer);

        m_pass_instances.world_init_pi.set_storage_buffer(0, m_buffers.chunk_hash_table);
        m_pass_instances.world_init_pi.set_storage_buffer(1, m_buffers.free_list);
        m_pass_instances.world_init_pi.set_storage_buffer(2, m_buffers.mesh_buffers_status);
        m_pass_instances.world_init_pi.set_storage_buffer(3, m_buffers.chunk_meta);
        m_pass_instances.world_init_pi.set_storage_buffer(4, m_buffers.enqueued);
        m_pass_instances.world_init_pi.set_storage_buffer(5, m_buffers.dirty_list);
        m_pass_instances.world_init_pi.set_storage_buffer(6, m_buffers.voxel_write_list);
        m_pass_instances.world_init_pi.set_storage_buffer(7, m_buffers.indirect_cmds);
        m_pass_instances.world_init_pi.set_storage_buffer(8, m_buffers.failed_dirty_list);

        m_pass_instances.world_init_pi.bind(m_command_buffer);

        m_pass_instances.world_init_pi.push_constants(m_command_buffer, WorldInitPushConstants{
            .u_chunk_hash_table_size = m_params.chunk_hash_table_size,
            .u_max_chunks = m_params.count_active_chunks
        });

        uint32_t max_items = std::max(m_params.chunk_hash_table_size, m_params.count_active_chunks);
        uint32_t groups_x = math_utils::div_up_u32(max_items, 256u);
        m_command_buffer.dispatch(groups_x, 1, 1);

        m_buffers.chunk_hash_table.memory_barrier(
            m_command_buffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
        );
        m_buffers.free_list.memory_barrier(
            m_command_buffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
        );
    }

    submit_compute_commands();
}

// void VoxelGridGPU::init_mesh_pool() {
//     // Pass 1: mesh_pool_clear
//     vb_heads_.bind_base_as_ssbo(0);
//     vb_state_.bind_base_as_ssbo(1);
//     vb_free_nodes_list_.bind_base_as_ssbo(2);

//     ib_heads_.bind_base_as_ssbo(3);
//     ib_state_.bind_base_as_ssbo(4);
//     ib_free_nodes_list_.bind_base_as_ssbo(5);

//     chunk_mesh_alloc_.bind_base_as_ssbo(6);

//     prog_mesh_pool_clear_.use();

//     glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_vb_pages"), count_vb_pages_);
//     glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_ib_pages"), count_ib_pages_);
//     glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_vb_nodes"), count_vb_nodes_);
//     glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_ib_nodes"), count_ib_nodes_);
//     glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_vb_heads_count"), vb_order_ + 1);
//     glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_ib_heads_count"), ib_order_ + 1);
//     glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_max_chunks"), count_active_chunks);

//     uint32_t max_count = std::max({count_vb_pages_, count_ib_pages_, count_active_chunks, count_vb_nodes_, count_ib_nodes_});
//     uint32_t groups_x = math_utils::div_up_u32(max_count, 256u);
//     prog_mesh_pool_clear_.dispatch_compute(groups_x, 1, 1);
    
//     glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

//     // Pass 2: mesh_pool_seed
//     vb_heads_.bind_base_as_ssbo(0);
//     vb_nodes_.bind_base_as_ssbo(1);
//     vb_state_.bind_base_as_ssbo(2);
//     vb_free_nodes_list_.bind_base_as_ssbo(3);

//     ib_heads_.bind_base_as_ssbo(4);
//     ib_nodes_.bind_base_as_ssbo(5);
//     ib_state_.bind_base_as_ssbo(6);
//     ib_free_nodes_list_.bind_base_as_ssbo(7);

//     prog_mesh_pool_seed_.use();

//     glUniform1ui(glGetUniformLocation(prog_mesh_pool_seed_.id, "u_vb_max_order"), vb_order_);
//     glUniform1ui(glGetUniformLocation(prog_mesh_pool_seed_.id, "u_ib_max_order"), ib_order_);

//     prog_mesh_pool_seed_.dispatch_compute(1, 1, 1);

//     glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
// }

void VoxelGrid::init_mesh_pool() {
    LOG_METHOD();

    {
        auto scope = m_command_buffer.begin_scope();

        auto compute_write_to_compute_read_write = [&](VulkanBuffer& buffer) {
            buffer.memory_barrier(
                m_command_buffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
            );
        };

        // Pass 1: mesh_pool_clear

        MeshPoolClearUniform mesh_pool_clear_uniform {
            .u_vb_pages = m_params.count_vb_pages,
            .u_ib_pages = m_params.count_ib_pages,
            .u_vb_nodes = m_params.count_vb_nodes,
            .u_ib_nodes = m_params.count_ib_nodes,
            .u_vb_heads_count = m_params.vb_order + 1,
            .u_ib_heads_count = m_params.ib_order + 1,
            .u_max_chunks = m_params.count_active_chunks
        };

        m_buffers.mesh_pool_clear_uniform.upload(&mesh_pool_clear_uniform, sizeof(MeshPoolClearUniform));

        // Host wrote the uniform/parameter buffer, compute shader will read it.
        // m_buffers.mesh_pool_clear_uniform.memory_barrier(
        //     m_command_buffer,
        //     VK_PIPELINE_STAGE_HOST_BIT,
        //     VK_ACCESS_HOST_WRITE_BIT,
        //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        //     VK_ACCESS_SHADER_READ_BIT
        // );
        
        m_pass_instances.mesh_pool_clear_pi.set_storage_buffer(0, m_buffers.vb_heads);
        m_pass_instances.mesh_pool_clear_pi.set_storage_buffer(1, m_buffers.vb_state);
        m_pass_instances.mesh_pool_clear_pi.set_storage_buffer(2, m_buffers.vb_free_nodes_list);
        m_pass_instances.mesh_pool_clear_pi.set_storage_buffer(3, m_buffers.ib_heads);
        m_pass_instances.mesh_pool_clear_pi.set_storage_buffer(4, m_buffers.ib_state);
        m_pass_instances.mesh_pool_clear_pi.set_storage_buffer(5, m_buffers.ib_free_nodes_list);
        m_pass_instances.mesh_pool_clear_pi.set_storage_buffer(6, m_buffers.chunk_mesh_alloc);
        m_pass_instances.mesh_pool_clear_pi.set_uniform_buffer(7, m_buffers.mesh_pool_clear_uniform);

        m_pass_instances.mesh_pool_clear_pi.bind(m_command_buffer);

        uint32_t max_count = std::max({m_params.count_vb_pages, m_params.count_ib_pages, m_params.count_active_chunks, 
                                       m_params.count_vb_nodes, m_params.count_ib_nodes});
        uint32_t groups_x = math_utils::div_up_u32(max_count, 256u);

        m_command_buffer.dispatch(groups_x, 1, 1);

        compute_write_to_compute_read_write(m_buffers.vb_heads);
        compute_write_to_compute_read_write(m_buffers.vb_state);
        compute_write_to_compute_read_write(m_buffers.vb_free_nodes_list);

        compute_write_to_compute_read_write(m_buffers.ib_heads);
        compute_write_to_compute_read_write(m_buffers.ib_state);
        compute_write_to_compute_read_write(m_buffers.ib_free_nodes_list);

        compute_write_to_compute_read_write(m_buffers.chunk_mesh_alloc);


        // Pass 2: mesh_pool_seed

        MeshPoolSeedUniform mesh_pool_seed_uniform {
            .u_vb_max_order = m_params.vb_order,
            .u_ib_max_order = m_params.ib_order
        };

        m_buffers.mesh_pool_seed_uniform.upload(&mesh_pool_seed_uniform, sizeof(MeshPoolSeedUniform));

        m_pass_instances.mesh_pool_seed_pi.set_storage_buffer(0, m_buffers.vb_heads);
        m_pass_instances.mesh_pool_seed_pi.set_storage_buffer(1, m_buffers.vb_nodes);
        m_pass_instances.mesh_pool_seed_pi.set_storage_buffer(2, m_buffers.vb_state);
        m_pass_instances.mesh_pool_seed_pi.set_storage_buffer(3, m_buffers.vb_free_nodes_list);
        m_pass_instances.mesh_pool_seed_pi.set_storage_buffer(4, m_buffers.ib_heads);
        m_pass_instances.mesh_pool_seed_pi.set_storage_buffer(5, m_buffers.ib_nodes);
        m_pass_instances.mesh_pool_seed_pi.set_storage_buffer(6, m_buffers.ib_state);
        m_pass_instances.mesh_pool_seed_pi.set_storage_buffer(7, m_buffers.ib_free_nodes_list);
        m_pass_instances.mesh_pool_seed_pi.set_uniform_buffer(8, m_buffers.mesh_pool_seed_uniform);

        m_pass_instances.mesh_pool_seed_pi.bind(m_command_buffer);

        m_command_buffer.dispatch(1, 1, 1);

        compute_write_to_compute_read_write(m_buffers.vb_heads);
        compute_write_to_compute_read_write(m_buffers.vb_nodes);
        compute_write_to_compute_read_write(m_buffers.vb_state);
        compute_write_to_compute_read_write(m_buffers.vb_free_nodes_list);

        compute_write_to_compute_read_write(m_buffers.ib_heads);
        compute_write_to_compute_read_write(m_buffers.ib_nodes);
        compute_write_to_compute_read_write(m_buffers.ib_state);
        compute_write_to_compute_read_write(m_buffers.ib_free_nodes_list);
    }

    submit_compute_commands();
}

// void VoxelGridGPU::init_draw_buffers() {
//     static VertexLayout vertex_layout;
//     if (vertex_layout.attributes.size() == 0) {
//         vertex_layout.add(
//             "position",
//             0, 4, GL_FLOAT, GL_FALSE,
//             sizeof(VertexGPU),
//             offsetof(VertexGPU, pos), 
//             0, {0.0f, 0.0f, 0.0f, 1.0f}
//         );
//         vertex_layout.add(
//             "color",
//             1, 1, GL_UNSIGNED_INT, GL_FALSE,
//             sizeof(VertexGPU),
//             offsetof(VertexGPU, color), 
//             0, {0xffffffffu} // белый
//         );
//         vertex_layout.add(
//             "face",
//             2, 1, GL_UNSIGNED_INT, GL_FALSE,
//             sizeof(VertexGPU),
//             offsetof(VertexGPU, face), 
//             0, {0u} // Направление 0 (хз куда это, вверх мб?)
//         );
//     }

//     vao.setup(global_vertex_buffer_, global_index_buffer_, vertex_layout);
// }

// void VoxelGrid::init_draw_buffers() {

// }

void VoxelGrid::apply_writes_to_world_gpu(uint32_t write_count) {
    LOG_METHOD();

    if (write_count == 0u) {
        return;
    }

    logger.check(write_count <= m_params.max_write_count, "write_count exceeded max_write_count");

    {
        auto scope = m_command_buffer.begin_scope();

        m_buffers.voxel_write_list.memory_barrier(
            m_command_buffer,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_ACCESS_HOST_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );

        m_pass_instances.apply_writes_to_world_pi.set_storage_buffer(0, m_buffers.chunk_hash_table);
        m_pass_instances.apply_writes_to_world_pi.set_storage_buffer(1, m_buffers.voxel_write_list);
        m_pass_instances.apply_writes_to_world_pi.set_storage_buffer(2, m_buffers.voxels);
        m_pass_instances.apply_writes_to_world_pi.set_storage_buffer(3, m_buffers.free_list);
        m_pass_instances.apply_writes_to_world_pi.set_storage_buffer(4, m_buffers.chunk_meta);
        m_pass_instances.apply_writes_to_world_pi.set_storage_buffer(5, m_buffers.enqueued);
        m_pass_instances.apply_writes_to_world_pi.set_storage_buffer(6, m_buffers.dirty_list);

        m_pass_instances.apply_writes_to_world_pi.bind(m_command_buffer);

        m_pass_instances.apply_writes_to_world_pi.push_constants(m_command_buffer, ApplyVoxelWritesPushConstants{
            .u_chunk_hash_table_size = m_params.chunk_hash_table_size,
            .u_chunk_dim_x = m_params.chunk_size.x,
            .u_chunk_dim_y = m_params.chunk_size.y,
            .u_chunk_dim_z = m_params.chunk_size.z,
            .u_voxels_per_chunk = static_cast<uint32_t>(vox_per_chunk()),
            .u_pack_bits = math_utils::BITS,
            .u_pack_offset = static_cast<int32_t>(math_utils::OFFSET)
        });

        m_command_buffer.dispatch(math_utils::div_up_u32(write_count, 256u), 1, 1);

        m_buffers.voxels.memory_barrier(
            m_command_buffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
        );
        m_buffers.dirty_list.memory_barrier(
            m_command_buffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
        );
    }

    submit_compute_commands();
}

void VoxelGrid::apply_writes_to_world_from_cpu(
    const std::vector<glm::ivec3>& positions,
    const std::vector<VoxelDataGPU>& voxels)
{
    LOG_METHOD();

    logger.check(positions.size() == voxels.size(), "positions and voxels must have the same size");
    logger.check(positions.size() <= m_params.max_write_count, "CPU voxel write batch exceeded max_write_count");

    uint32_t write_count = static_cast<uint32_t>(positions.size());
    m_buffers.voxel_write_list.upload(&write_count, sizeof(write_count), 0);

    if (write_count == 0u) {
        return;
    }

    std::vector<VoxelWriteGPU> writes(write_count);
    for (uint32_t i = 0; i < write_count; ++i) {
        writes[i].world_voxel = glm::ivec4(positions[i], 0);
        writes[i].voxel_data = voxels[i];
        writes[i].set_flags = OVERWRITE_BIT;
        writes[i].pad1 = 0u;
    }

    m_buffers.voxel_write_list.upload(writes, sizeof(uint32_t) * 4);
    apply_writes_to_world_gpu(write_count);
}

void VoxelGrid::submit_compute_commands() {
    LOG_METHOD();

    logger.check(m_queue != nullptr, "VoxelGrid queue was not initialized");

    m_fence.reset();
    m_queue->submit(m_command_buffer, &m_fence);
    m_fence.wait();
    m_command_buffer.reset();
}
