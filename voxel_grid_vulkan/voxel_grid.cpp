#include "voxel_grid.h"

#include <string>
#include <algorithm>

#include "../math_utils.h"
#include "../managers/compute_pass_manager.h"
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
    const VoxelGridDesc& desc) 
    :   m_command_pool(device, queue),
        m_command_buffer(device, m_command_pool),
        m_fence(device),
        m_queue(&queue),
        m_compute_pass_manager(&compute_pass_manager),
        // m_buffer_filler(physical_device, device, compute_pass_manager, 1024),
        m_params(create_params(desc)),
        m_pass_instances(create_pass_instances(compute_pass_manager)),
        m_buffers(create_buffers(physical_device, device, m_command_buffer))
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
    // init_mesh_pool();
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
        .apply_writes_to_world_pi = PassInstance(compute_pass_manager.apply_writes_to_world_cp, dp)
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
        )
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
