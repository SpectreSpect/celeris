#include "voxel_grid.h"

#include <string>
#include <algorithm>
#include <utility>

#include "../math_utils.h"
#include "../managers/compute_pass_manager.h"
#include "../managers/material_instance_manager.h"
#include "../managers/texture_manager.h"
#include "voxel_grid_structures.h"
#include "../vulkan_self/vulkan_physical_device.h"
#include "../vulkan_self/vulkan_device.h"
#include "../vulkan_self/descriptor_set/descriptor_pool.h"
#include "../vulkan_self/vulkan_queue.h"
#include "../vulkan_self/push_constants_structures.h"
#include "../camera/camera.h"
#include "../renderer/point_cloud/point_cloud.h"
#include "../renderer/point_cloud/point_instance.h"
#include "../math_utils.h"

#include "shader_helper/buffer_dispatch_arg.h"

VoxelGridChunk::VoxelGridChunk(glm::uvec3 chunk_size, std::vector<VoxelDataGPU> voxels)
    : m_chunk_size(chunk_size), m_voxels(std::move(voxels)) {}

glm::uvec3 VoxelGridChunk::chunk_size() const noexcept {
    return m_chunk_size;
}

uint32_t VoxelGridChunk::voxel_count() const noexcept {
    return static_cast<uint32_t>(m_voxels.size());
}

const VoxelDataGPU& VoxelGridChunk::voxel(uint32_t x, uint32_t y, uint32_t z) const {
    return m_voxels[voxel_index(x, y, z)];
}

VoxelDataGPU& VoxelGridChunk::voxel(uint32_t x, uint32_t y, uint32_t z) {
    return m_voxels[voxel_index(x, y, z)];
}

const VoxelDataGPU& VoxelGridChunk::voxel(glm::uvec3 local_pos) const {
    return voxel(local_pos.x, local_pos.y, local_pos.z);
}

VoxelDataGPU& VoxelGridChunk::voxel(glm::uvec3 local_pos) {
    return voxel(local_pos.x, local_pos.y, local_pos.z);
}

const std::vector<VoxelDataGPU>& VoxelGridChunk::voxels() const noexcept {
    return m_voxels;
}

std::vector<VoxelDataGPU>& VoxelGridChunk::voxels() noexcept {
    return m_voxels;
}

uint32_t VoxelGridChunk::voxel_index(uint32_t x, uint32_t y, uint32_t z) const {
    if (x >= m_chunk_size.x || y >= m_chunk_size.y || z >= m_chunk_size.z) {
        throw std::out_of_range("VoxelGridChunk voxel position is out of bounds");
    }

    return x + m_chunk_size.x * (y + m_chunk_size.y * z);
}

VoxelGrid::VoxelGrid(
    const VulkanPhysicalDevice& physical_device,
    VulkanDevice& device,
    VulkanQueue& queue,
    ComputePassManager& compute_pass_manager,
    MaterialInstanceManager& material_instance_manager,
    const VoxelGridDesc& desc,
    VkMemoryPropertyFlags ssbo_memory_properties) 
    :   m_command_pool(device, queue),
        m_command_buffer(device, m_command_pool),
        m_fence(device),
        m_queue(&queue),
        m_params(create_params(desc)),
        m_pass_instances(create_pass_instances(device, compute_pass_manager)),
        m_buffers(create_buffers(physical_device, device, m_command_buffer, ssbo_memory_properties)),
        m_mesh_view(m_buffers.global_vertex_buffer.get_view(), m_buffers.global_index_buffer.get_view(), desc.max_quads * 6u),
        m_render_object(m_mesh_view, material_instance_manager.voxel_pbr, m_buffers.indirect_cmds, m_params.count_active_chunks),
        m_shader_helper(device, compute_pass_manager)
{
    LOG_METHOD();
    m_render_object.set_material_data(PBRMaterialData::with_pbr_maps(
        TextureManager::st_peters_square_night_4k_pbr_map_id,
        0.0f,
        0.95f,
        1.8f,
        glm::vec4(1.0f),
        1.0f
    ));

    world_init_gpu();
    init_mesh_pool();
}

void VoxelGrid::conditional_prepare_rebuild(VulkanCommandBuffer& command_buffer, VulkanBuffer& clear_dispatch_args, VulkanBuffer& fill_dispatch_args) {
    LOG_METHOD();

    m_pass_instances.hash_table_conditional_dispatch_adapter_pw.set_storage_buffer(0, m_buffers.chunk_hash_table);
    m_pass_instances.hash_table_conditional_dispatch_adapter_pw.set_storage_buffer(1, clear_dispatch_args);
    m_pass_instances.hash_table_conditional_dispatch_adapter_pw.set_storage_buffer(2, fill_dispatch_args);

    m_pass_instances.hash_table_conditional_dispatch_adapter_pw.bind(command_buffer);

    m_pass_instances.hash_table_conditional_dispatch_adapter_pw.push_constants(command_buffer, HashTableConditionalDispatchAdapterPushConstants{
        .u_chunk_hash_table_size = m_params.chunk_hash_table_size,
        .u_max_chunks = m_params.count_active_chunks,
        .u_tombs_to_rebuild = static_cast<uint32_t>(m_params.tomb_fraction_to_rebuild * m_params.chunk_hash_table_size)
    });

    command_buffer.dispatch(1, 1, 1);

    m_buffers.chunk_hash_table.memory_barrier_compute_write_to_compute_write_read(command_buffer);

    clear_dispatch_args.memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        VK_ACCESS_INDIRECT_COMMAND_READ_BIT
    );

    fill_dispatch_args.memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        VK_ACCESS_INDIRECT_COMMAND_READ_BIT
    );
}

void VoxelGrid::clear_chunk_hash_table(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args) {
    LOG_METHOD();

    m_pass_instances.clear_chunk_hash_table_pi.set_storage_buffer(0, m_buffers.chunk_hash_table);

    m_pass_instances.clear_chunk_hash_table_pi.bind(command_buffer);

    m_pass_instances.clear_chunk_hash_table_pi.push_constants(command_buffer, ClearChunkHashTablePushConstants{
        .u_chunk_hash_table_size = m_params.chunk_hash_table_size
    });

    command_buffer.dispatch_indirect(dispatch_args);

    m_buffers.chunk_hash_table.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::fill_chunk_hash_table(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args, uint32_t pack_bits, int pack_offset) {
    LOG_METHOD();

    m_pass_instances.fill_chunk_hash_table_pi.set_storage_buffer(0, m_buffers.chunk_hash_table);
    m_pass_instances.fill_chunk_hash_table_pi.set_storage_buffer(1, m_buffers.chunk_meta);
    m_pass_instances.fill_chunk_hash_table_pi.set_storage_buffer(2, m_buffers.enqueued);

    m_pass_instances.fill_chunk_hash_table_pi.bind(command_buffer);

    m_pass_instances.fill_chunk_hash_table_pi.push_constants(command_buffer, FillChunkHashTablePushConstants{
        .u_max_chunks = m_params.count_active_chunks,
        .u_chunk_hash_table_size = m_params.chunk_hash_table_size,
        .u_pack_bits = pack_bits,
        .u_pack_offset = pack_offset
    });

    command_buffer.dispatch_indirect(dispatch_args);

    m_buffers.chunk_hash_table.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.enqueued.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::rebuild_chunk_hash_table(VulkanCommandBuffer& command_buffer, uint32_t pack_bits, int pack_offset) {
    LOG_METHOD();

    conditional_prepare_rebuild(command_buffer, m_buffers.dispatch_args, m_buffers.dispatch_args_additional);

    clear_chunk_hash_table(command_buffer, m_buffers.dispatch_args);
    fill_chunk_hash_table(command_buffer, m_buffers.dispatch_args_additional, pack_bits, pack_offset);
}

void VoxelGrid::reset_heads(VulkanCommandBuffer& command_buffer) {
    LOG_METHOD();

    BucketHead bucket_head;
    bucket_head.id = INVALID_ID;
    bucket_head.count = 0;

    m_buffers.bucket_heads.fill(command_buffer, m_pass_instances.fill_buffer_pw, bucket_head, sizeof(BucketHead) * m_params.count_evict_buckets);
    m_buffers.bucket_heads.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::build_bucket_lists(VulkanCommandBuffer& command_buffer, glm::vec3 cam_pos) {
    LOG_METHOD();

    m_pass_instances.evict_buckets_build_pi.set_storage_buffer(0, m_buffers.chunk_meta);
    m_pass_instances.evict_buckets_build_pi.set_storage_buffer(1, m_buffers.bucket_heads);
    m_pass_instances.evict_buckets_build_pi.set_storage_buffer(2, m_buffers.bucket_next);

    m_pass_instances.evict_buckets_build_pi.bind(command_buffer);

    m_pass_instances.evict_buckets_build_pi.push_constants(command_buffer, EvictBucketsBuildPushConstants{
        .u_cam_pos = glm::vec4(cam_pos.x, cam_pos.y, cam_pos.z, 0),
        .u_chunk_dim = glm::ivec4(m_params.chunk_size.x, m_params.chunk_size.y, m_params.chunk_size.z, 0),
        .u_voxel_size = glm::vec4(m_params.voxel_size.x, m_params.voxel_size.y, m_params.voxel_size.z, 0),

        .u_max_chunks = m_params.count_active_chunks,
        .u_bucket_count = m_params.count_evict_buckets,
        .u_pack_bits = math_utils::BITS,
        .u_pack_offset = math_utils::OFFSET,

        .f_eviction_bucket_shell_thickness = m_params.eviction_bucket_shell_thickness
    });

    uint32_t gx = math_utils::div_up_u32(m_params.count_active_chunks, 256u);
    command_buffer.dispatch(gx, 1, 1);

    m_buffers.bucket_heads.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.bucket_next.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::prepare_evict_lowpriority_chunks(VulkanCommandBuffer& command_buffer, VulkanBuffer& dispatch_args) {
    LOG_METHOD();

    m_pass_instances.evict_low_priority_dispatch_adapter_pw.set_storage_buffer(0, m_buffers.evicted_chunks_list);
    m_pass_instances.evict_low_priority_dispatch_adapter_pw.set_storage_buffer(1, dispatch_args);
    m_pass_instances.evict_low_priority_dispatch_adapter_pw.set_storage_buffer(2, m_buffers.free_list);

    m_pass_instances.evict_low_priority_dispatch_adapter_pw.bind(command_buffer);

    m_pass_instances.evict_low_priority_dispatch_adapter_pw.push_constants(command_buffer, EvictLowPriorityDispatchAdapterPushConstants{
        .u_min_free = m_params.min_free_chunks
    });

    command_buffer.dispatch(1, 1, 1);

    m_buffers.evicted_chunks_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    dispatch_args.memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        VK_ACCESS_INDIRECT_COMMAND_READ_BIT
    );
}

void VoxelGrid::evict_lowpriority_chunks(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args) {
    LOG_METHOD();

    m_pass_instances.evict_low_priority_pi.set_storage_buffer(0, m_buffers.chunk_hash_table);
    m_pass_instances.evict_low_priority_pi.set_storage_buffer(1, m_buffers.free_list);
    m_pass_instances.evict_low_priority_pi.set_storage_buffer(2, m_buffers.chunk_meta);
    m_pass_instances.evict_low_priority_pi.set_storage_buffer(3, m_buffers.enqueued);
    m_pass_instances.evict_low_priority_pi.set_storage_buffer(4, m_buffers.bucket_heads);
    m_pass_instances.evict_low_priority_pi.set_storage_buffer(5, m_buffers.bucket_next);
    m_pass_instances.evict_low_priority_pi.set_storage_buffer(6, m_buffers.chunk_mesh_alloc);
    m_pass_instances.evict_low_priority_pi.set_storage_buffer(7, m_buffers.evicted_chunks_list);

    m_pass_instances.evict_low_priority_pi.bind(command_buffer);

    m_pass_instances.evict_low_priority_pi.push_constants(command_buffer, EvictLowPriorityPushConstants{
        .u_chunk_hash_table_size = m_params.chunk_hash_table_size,
        .u_bucket_count = m_params.count_evict_buckets
    });

    command_buffer.dispatch_indirect(dispatch_args);

    m_buffers.chunk_hash_table.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.free_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.chunk_meta.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.enqueued.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.bucket_heads.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.bucket_next.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.chunk_mesh_alloc.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.evicted_chunks_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::free_evicted_chunks_mesh(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args) {
    LOG_METHOD();

    m_pass_instances.free_evicted_chunks_mesh_pi.set_storage_buffer(0, m_buffers.chunk_mesh_alloc);

    m_pass_instances.free_evicted_chunks_mesh_pi.set_storage_buffer(1, m_buffers.vb_mesh_allocator_buffers.heads);
    m_pass_instances.free_evicted_chunks_mesh_pi.set_storage_buffer(2, m_buffers.vb_mesh_allocator_buffers.state);
    m_pass_instances.free_evicted_chunks_mesh_pi.set_storage_buffer(3, m_buffers.vb_mesh_allocator_buffers.nodes);
    m_pass_instances.free_evicted_chunks_mesh_pi.set_storage_buffer(4, m_buffers.vb_mesh_allocator_buffers.free_nodes_list);
    m_pass_instances.free_evicted_chunks_mesh_pi.set_storage_buffer(5, m_buffers.vb_mesh_allocator_buffers.returned_nodes_list);

    m_pass_instances.free_evicted_chunks_mesh_pi.set_storage_buffer(6, m_buffers.ib_mesh_allocator_buffers.heads);
    m_pass_instances.free_evicted_chunks_mesh_pi.set_storage_buffer(7, m_buffers.ib_mesh_allocator_buffers.state);
    m_pass_instances.free_evicted_chunks_mesh_pi.set_storage_buffer(8, m_buffers.ib_mesh_allocator_buffers.nodes);
    m_pass_instances.free_evicted_chunks_mesh_pi.set_storage_buffer(9, m_buffers.ib_mesh_allocator_buffers.free_nodes_list);
    m_pass_instances.free_evicted_chunks_mesh_pi.set_storage_buffer(10, m_buffers.ib_mesh_allocator_buffers.returned_nodes_list);

    m_pass_instances.free_evicted_chunks_mesh_pi.set_storage_buffer(11, m_buffers.evicted_chunks_list);

    m_pass_instances.free_evicted_chunks_mesh_pi.bind(command_buffer);

    m_pass_instances.free_evicted_chunks_mesh_pi.push_constants(command_buffer, FreeEvictedChunksMeshPushConstants{
        .vb_max_order = m_params.vb_allocator_params.max_order,
        .ib_max_order = m_params.ib_allocator_params.max_order
    });

    command_buffer.dispatch_indirect(dispatch_args);

    m_buffers.chunk_mesh_alloc.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.vb_mesh_allocator_buffers.heads.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.vb_mesh_allocator_buffers.state.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.vb_mesh_allocator_buffers.nodes.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.vb_mesh_allocator_buffers.free_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.vb_mesh_allocator_buffers.returned_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);

    m_buffers.ib_mesh_allocator_buffers.heads.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.ib_mesh_allocator_buffers.state.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.ib_mesh_allocator_buffers.nodes.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.ib_mesh_allocator_buffers.free_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.ib_mesh_allocator_buffers.returned_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.evicted_chunks_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::reset_evicted_list_and_buckets(VulkanCommandBuffer& command_buffer) {
    LOG_METHOD();

    m_pass_instances.reset_evicted_list_and_buckets_pi.set_storage_buffer(0, m_buffers.bucket_heads);
    m_pass_instances.reset_evicted_list_and_buckets_pi.set_storage_buffer(1, m_buffers.evicted_chunks_list);
    m_pass_instances.reset_evicted_list_and_buckets_pi.set_storage_buffer(2, m_buffers.free_list);

    m_pass_instances.reset_evicted_list_and_buckets_pi.bind(command_buffer);

    m_pass_instances.reset_evicted_list_and_buckets_pi.push_constants(command_buffer, ResetEvictedListAndBucketsPushConstants{
        .u_bucket_count = m_params.count_evict_buckets
    });

    uint32_t bucket_count_groups = math_utils::div_up_u32(m_params.count_evict_buckets, 256u);
    command_buffer.dispatch(bucket_count_groups, 1, 1);

    m_buffers.bucket_heads.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.evicted_chunks_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.free_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::ensure_free_chunks_gpu(VulkanCommandBuffer& command_buffer, glm::vec3 cam_pos, uint32_t pack_bits, int pack_offset) {
    LOG_METHOD();
    
    reset_heads(command_buffer);

    build_bucket_lists(command_buffer, cam_pos);
    
    prepare_evict_lowpriority_chunks(command_buffer, m_buffers.dispatch_args);
    evict_lowpriority_chunks(command_buffer, m_buffers.dispatch_args);
    
    free_evicted_chunks_mesh(command_buffer, m_buffers.dispatch_args); // dispatch_args здесь уже подготовлен

    reset_evicted_list_and_buckets(command_buffer);

    prepare_return_free_alloc_nodes(command_buffer, m_buffers.dispatch_args);
    return_free_alloc_nodes(command_buffer, m_buffers.dispatch_args);

    rebuild_chunk_hash_table(command_buffer, pack_bits, pack_offset);
}

void VoxelGrid::mesh_reset(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args) {
    LOG_METHOD();

    m_pass_instances.mesh_reset_pi.set_storage_buffer(0, m_buffers.dirty_list);
    m_pass_instances.mesh_reset_pi.set_storage_buffer(1, m_buffers.dirty_quad_count);
    m_pass_instances.mesh_reset_pi.set_storage_buffer(2, m_buffers.emit_counters);

    m_pass_instances.mesh_reset_pi.bind(command_buffer);

    command_buffer.dispatch_indirect(dispatch_args);

    m_buffers.dirty_quad_count.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.emit_counters.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::mesh_count(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args, uint32_t pack_bits, int pack_offset) {
    LOG_METHOD();

    m_pass_instances.mesh_count_pi.set_storage_buffer(0, m_buffers.chunk_hash_table);
    m_pass_instances.mesh_count_pi.set_storage_buffer(1, m_buffers.voxels);
    m_pass_instances.mesh_count_pi.set_storage_buffer(2, m_buffers.dirty_list);
    m_pass_instances.mesh_count_pi.set_storage_buffer(3, m_buffers.dirty_quad_count);
    m_pass_instances.mesh_count_pi.set_storage_buffer(4, m_buffers.chunk_meta);

    m_pass_instances.mesh_count_pi.push_constants(command_buffer, MeshCountPushConstants{
        .u_chunk_dim = glm::ivec4(m_params.chunk_size, 0),
        .u_chunk_hash_table_size = m_params.chunk_hash_table_size,
        .u_voxels_per_chunk = static_cast<uint32_t>(vox_per_chunk()),
        .u_pack_bits = pack_bits,
        .u_pack_offset = pack_offset,
        .display_inflated_voxels = m_params.display_inflated_voxels
    });

    m_pass_instances.mesh_count_pi.bind(command_buffer);

    command_buffer.dispatch_indirect(dispatch_args);

    m_buffers.chunk_hash_table.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.dirty_quad_count.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::reset_allocation_retry_list(VulkanCommandBuffer& command_buffer, VulkanBuffer& allocation_retry_list) {
    LOG_METHOD();

    allocation_retry_list.fill(command_buffer, 0u, sizeof(uint32_t));
    allocation_retry_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::mesh_alloc_buffer(
    VulkanCommandBuffer& command_buffer, 
    const VulkanBuffer& dispatch_args,
    AllocatorBuffers& mesh_allocator_buffers,
    BuddyAllocatorParams& mesh_allocator_params,
    uint32_t quad_size,
    bool is_vertex_phase)
{
    LOG_METHOD();

    m_pass_instances.mesh_alloc_pw.set_storage_buffer(0, m_buffers.mesh_buffers_status);
    m_pass_instances.mesh_alloc_pw.set_storage_buffer(1, m_buffers.dirty_list);
    m_pass_instances.mesh_alloc_pw.set_storage_buffer(2, m_buffers.dirty_quad_count);
    m_pass_instances.mesh_alloc_pw.set_storage_buffer(3, m_buffers.chunk_meta);
    m_pass_instances.mesh_alloc_pw.set_storage_buffer(4, m_buffers.chunk_mesh_alloc_local);
    m_pass_instances.mesh_alloc_pw.set_storage_buffer(5, m_buffers.chunk_mesh_alloc);

    m_pass_instances.mesh_alloc_pw.set_storage_buffer(6, mesh_allocator_buffers.heads);
    m_pass_instances.mesh_alloc_pw.set_storage_buffer(7, mesh_allocator_buffers.state);
    m_pass_instances.mesh_alloc_pw.set_storage_buffer(8, mesh_allocator_buffers.nodes);
    m_pass_instances.mesh_alloc_pw.set_storage_buffer(9, mesh_allocator_buffers.free_nodes_list);
    m_pass_instances.mesh_alloc_pw.set_storage_buffer(10, mesh_allocator_buffers.returned_nodes_list);
    
    m_pass_instances.mesh_alloc_pw.set_storage_buffer(11, m_buffers.active_splitters);
    m_pass_instances.mesh_alloc_pw.set_storage_buffer(12, m_buffers.debug_counter);

    m_pass_instances.mesh_alloc_pw.set_storage_buffer(13, m_buffers.allocation_retry_list);

    m_pass_instances.mesh_alloc_pw.push_constants(command_buffer, MeshAllocPushConstants{
        .bb_pages = mesh_allocator_params.count_pages,
        .bb_page_elements = mesh_allocator_params.page_size,
        .bb_max_order = mesh_allocator_params.max_order,
        .bb_quad_size = quad_size,
        .u_is_vb_phase = is_vertex_phase ? 1u : 0u
    });

    m_pass_instances.mesh_alloc_pw.bind(command_buffer);

    command_buffer.dispatch_indirect(dispatch_args);

    m_buffers.mesh_buffers_status.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.chunk_mesh_alloc_local.memory_barrier_compute_write_to_compute_write_read(command_buffer);

    mesh_allocator_buffers.heads.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    mesh_allocator_buffers.state.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    mesh_allocator_buffers.nodes.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    mesh_allocator_buffers.free_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    mesh_allocator_buffers.returned_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);

    m_buffers.debug_counter.memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_HOST_BIT,
        VK_ACCESS_HOST_READ_BIT
    );
}

void VoxelGrid::retry_mesh_alloc(
    VulkanCommandBuffer& command_buffer,
    const VulkanBuffer& dispatch_args,
    VulkanBuffer& readable_retry_list,
    VulkanBuffer& writable_retry_list,
    AllocatorBuffers& mesh_allocator_buffers,
    BuddyAllocatorParams& mesh_allocator_params,
    uint32_t quad_size,
    bool is_vertex_phase)
{
    LOG_METHOD();

    m_pass_instances.retry_mesh_alloc_pw.set_storage_buffer(0, readable_retry_list);
    m_pass_instances.retry_mesh_alloc_pw.set_storage_buffer(1, writable_retry_list);
    m_pass_instances.retry_mesh_alloc_pw.set_storage_buffer(2, m_buffers.dirty_list);
    m_pass_instances.retry_mesh_alloc_pw.set_storage_buffer(3, m_buffers.dirty_quad_count);
    m_pass_instances.retry_mesh_alloc_pw.set_storage_buffer(4, m_buffers.chunk_meta);
    m_pass_instances.retry_mesh_alloc_pw.set_storage_buffer(5, m_buffers.chunk_mesh_alloc_local);
    m_pass_instances.retry_mesh_alloc_pw.set_storage_buffer(6, mesh_allocator_buffers.heads);
    m_pass_instances.retry_mesh_alloc_pw.set_storage_buffer(7, mesh_allocator_buffers.state);
    m_pass_instances.retry_mesh_alloc_pw.set_storage_buffer(8, mesh_allocator_buffers.nodes);
    m_pass_instances.retry_mesh_alloc_pw.set_storage_buffer(9, mesh_allocator_buffers.free_nodes_list);
    m_pass_instances.retry_mesh_alloc_pw.set_storage_buffer(10, mesh_allocator_buffers.returned_nodes_list);

    m_pass_instances.retry_mesh_alloc_pw.bind(command_buffer);

    m_pass_instances.retry_mesh_alloc_pw.push_constants(command_buffer, RetryMeshAllocPushConstants{
        .bb_pages = mesh_allocator_params.count_pages,
        .bb_page_elements = mesh_allocator_params.page_size,
        .bb_max_order = mesh_allocator_params.max_order,
        .bb_quad_size = quad_size,
        .u_is_vb_phase = is_vertex_phase
    });

    command_buffer.dispatch_indirect(dispatch_args);

    writable_retry_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.chunk_mesh_alloc_local.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    mesh_allocator_buffers.heads.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    mesh_allocator_buffers.state.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    mesh_allocator_buffers.nodes.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    mesh_allocator_buffers.free_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    mesh_allocator_buffers.returned_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::mesh_alloc(
    VulkanCommandBuffer& command_buffer,
    VulkanBuffer& dispatch_args,
    AllocatorBuffers& mesh_allocator_buffers,
    BuddyAllocatorParams& mesh_allocator_params,
    uint32_t quad_size,
    bool is_vertex_phase)
{
    reset_allocation_retry_list(command_buffer, m_buffers.allocation_retry_list);
    
    m_shader_helper.prepare_dispatch_args(command_buffer, dispatch_args, BufferDispatchArg(&m_buffers.dirty_list, 0u));
    mesh_alloc_buffer(
        command_buffer, 
        dispatch_args,
        mesh_allocator_buffers,
        mesh_allocator_params,
        quad_size,
        is_vertex_phase
    );

    VulkanBuffer* readable_retry_list = &m_buffers.allocation_retry_list;
    VulkanBuffer* writable_retry_list = &m_buffers.allocation_retry_list_additional;
    for (uint32_t retry_attempt = 0u; retry_attempt < m_params.count_allocation_retry_attempts; retry_attempt++) {
        reset_allocation_retry_list(command_buffer, *writable_retry_list);

        m_shader_helper.prepare_dispatch_args(command_buffer, dispatch_args, BufferDispatchArg(readable_retry_list));
        retry_mesh_alloc(
            command_buffer,
            dispatch_args,
            *readable_retry_list,
            *writable_retry_list,
            mesh_allocator_buffers,
            mesh_allocator_params,
            quad_size,
            is_vertex_phase
        );

        std::swap(readable_retry_list, writable_retry_list);
    }
}

void VoxelGrid::verify_mesh_allocation(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args) {
    m_pass_instances.verify_mesh_allocation_pi.set_storage_buffer(0, m_buffers.chunk_mesh_alloc_local);
    m_pass_instances.verify_mesh_allocation_pi.set_storage_buffer(1, m_buffers.chunk_mesh_alloc);
    m_pass_instances.verify_mesh_allocation_pi.set_storage_buffer(2, m_buffers.dirty_list);
    m_pass_instances.verify_mesh_allocation_pi.set_storage_buffer(3, m_buffers.mesh_buffers_status);

    m_pass_instances.verify_mesh_allocation_pi.set_storage_buffer(4, m_buffers.vb_mesh_allocator_buffers.heads);
    m_pass_instances.verify_mesh_allocation_pi.set_storage_buffer(5, m_buffers.vb_mesh_allocator_buffers.state);
    m_pass_instances.verify_mesh_allocation_pi.set_storage_buffer(6, m_buffers.vb_mesh_allocator_buffers.nodes);
    m_pass_instances.verify_mesh_allocation_pi.set_storage_buffer(7, m_buffers.vb_mesh_allocator_buffers.free_nodes_list);
    m_pass_instances.verify_mesh_allocation_pi.set_storage_buffer(8, m_buffers.vb_mesh_allocator_buffers.returned_nodes_list);

    m_pass_instances.verify_mesh_allocation_pi.set_storage_buffer(9, m_buffers.ib_mesh_allocator_buffers.heads);
    m_pass_instances.verify_mesh_allocation_pi.set_storage_buffer(10, m_buffers.ib_mesh_allocator_buffers.state);
    m_pass_instances.verify_mesh_allocation_pi.set_storage_buffer(11, m_buffers.ib_mesh_allocator_buffers.nodes);
    m_pass_instances.verify_mesh_allocation_pi.set_storage_buffer(12, m_buffers.ib_mesh_allocator_buffers.free_nodes_list);
    m_pass_instances.verify_mesh_allocation_pi.set_storage_buffer(13, m_buffers.ib_mesh_allocator_buffers.returned_nodes_list);

    m_pass_instances.verify_mesh_allocation_pi.push_constants(command_buffer, VerifyMeshAllocationPushConstants{
        .vb_max_order = m_params.vb_allocator_params.max_order,
        .ib_max_order = m_params.ib_allocator_params.max_order
    });

    m_pass_instances.verify_mesh_allocation_pi.bind(command_buffer);

    command_buffer.dispatch_indirect(dispatch_args);

    m_buffers.chunk_mesh_alloc.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.mesh_buffers_status.memory_barrier_compute_write_to_compute_write_read(command_buffer);

    m_buffers.vb_mesh_allocator_buffers.heads.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.vb_mesh_allocator_buffers.state.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.vb_mesh_allocator_buffers.nodes.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.vb_mesh_allocator_buffers.free_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.vb_mesh_allocator_buffers.returned_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);

    m_buffers.ib_mesh_allocator_buffers.heads.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.ib_mesh_allocator_buffers.state.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.ib_mesh_allocator_buffers.nodes.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.ib_mesh_allocator_buffers.free_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.ib_mesh_allocator_buffers.returned_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::prepare_return_free_alloc_nodes(VulkanCommandBuffer& command_buffer, VulkanBuffer& dispatch_args) {
    m_pass_instances.return_free_alloc_nodes_dispatch_adapter_pw.set_storage_buffer(0, m_buffers.vb_mesh_allocator_buffers.returned_nodes_list);
    m_pass_instances.return_free_alloc_nodes_dispatch_adapter_pw.set_storage_buffer(1, m_buffers.ib_mesh_allocator_buffers.returned_nodes_list);
    m_pass_instances.return_free_alloc_nodes_dispatch_adapter_pw.set_storage_buffer(2, dispatch_args);

    m_pass_instances.return_free_alloc_nodes_dispatch_adapter_pw.bind(command_buffer);

    command_buffer.dispatch(1, 1, 1);

    m_buffers.vb_mesh_allocator_buffers.returned_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.ib_mesh_allocator_buffers.returned_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);

    dispatch_args.memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        VK_ACCESS_INDIRECT_COMMAND_READ_BIT
    );
}

void VoxelGrid::return_free_alloc_nodes(VulkanCommandBuffer& command_buffer, VulkanBuffer& dispatch_args) {
    m_pass_instances.return_free_alloc_nodes_pw.set_storage_buffer(0, m_buffers.vb_mesh_allocator_buffers.free_nodes_list);
    m_pass_instances.return_free_alloc_nodes_pw.set_storage_buffer(1, m_buffers.vb_mesh_allocator_buffers.returned_nodes_list);

    m_pass_instances.return_free_alloc_nodes_pw.set_storage_buffer(2, m_buffers.ib_mesh_allocator_buffers.free_nodes_list);
    m_pass_instances.return_free_alloc_nodes_pw.set_storage_buffer(3, m_buffers.ib_mesh_allocator_buffers.returned_nodes_list);

    m_pass_instances.return_free_alloc_nodes_pw.push_constants(command_buffer, ReturnFreeAllocNodesPushConstants{
        .u3_chunk_size = glm::uvec4(m_params.chunk_size, 0)
    });

    m_pass_instances.return_free_alloc_nodes_pw.bind(command_buffer);

    command_buffer.dispatch_indirect(dispatch_args);

    m_buffers.vb_mesh_allocator_buffers.free_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.vb_mesh_allocator_buffers.returned_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);

    m_buffers.ib_mesh_allocator_buffers.free_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.ib_mesh_allocator_buffers.returned_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::mesh_emit(VulkanCommandBuffer& command_buffer, VulkanBuffer& dispatch_args, uint32_t pack_bits, int pack_offset) {
    m_pass_instances.mesh_emit_pi.set_storage_buffer(0, m_buffers.chunk_hash_table);
    m_pass_instances.mesh_emit_pi.set_storage_buffer(1, m_buffers.voxels);
    m_pass_instances.mesh_emit_pi.set_storage_buffer(2, m_buffers.mesh_buffers_status);
    m_pass_instances.mesh_emit_pi.set_storage_buffer(3, m_buffers.dirty_list);
    m_pass_instances.mesh_emit_pi.set_storage_buffer(4, m_buffers.emit_counters);
    m_pass_instances.mesh_emit_pi.set_storage_buffer(5, m_buffers.chunk_mesh_alloc);

    m_pass_instances.mesh_emit_pi.set_storage_buffer(6, m_buffers.chunk_meta);

    m_pass_instances.mesh_emit_pi.set_storage_buffer(7, m_buffers.global_vertex_buffer);
    m_pass_instances.mesh_emit_pi.set_storage_buffer(8, m_buffers.global_index_buffer);

    m_pass_instances.mesh_emit_pi.push_constants(command_buffer, MeshEmitPushConstants{
        .u_chunk_dim = glm::uvec4(m_params.chunk_size, 0),
        .u_voxel_size = glm::vec4(m_params.voxel_size, 0.0f),

        .u_pack_bits = pack_bits,
        .u_pack_offset = pack_offset,

        .u_vb_page_size_bytes = m_params.vb_allocator_params.page_size,
        .u_ib_page_size_bytes = m_params.ib_allocator_params.page_size,

        .u_chunk_hash_table_size = m_params.chunk_hash_table_size,
        .u_voxels_per_chunk = static_cast<uint32_t>(vox_per_chunk()),
        .display_inflated_voxels = m_params.display_inflated_voxels,
        .inflated_voxel_color = m_params.inflated_voxel_color
    });

    m_pass_instances.mesh_emit_pi.bind(command_buffer);

    command_buffer.dispatch_indirect(dispatch_args);

    m_buffers.chunk_hash_table.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.mesh_buffers_status.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.emit_counters.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.chunk_mesh_alloc.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    // m_buffers.global_vertex_buffer.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    // m_buffers.global_index_buffer.memory_barrier_compute_write_to_compute_write_read(command_buffer);

    m_buffers.global_vertex_buffer.memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
    );

    m_buffers.global_index_buffer.memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VK_ACCESS_INDEX_READ_BIT
    );
}

void VoxelGrid::mesh_finalize(VulkanCommandBuffer& command_buffer, VulkanBuffer& dispatch_args) {
    m_pass_instances.mesh_finalize_pi.set_storage_buffer(0, m_buffers.dirty_list);
    m_pass_instances.mesh_finalize_pi.set_storage_buffer(1, m_buffers.enqueued);
    m_pass_instances.mesh_finalize_pi.set_storage_buffer(2, m_buffers.chunk_meta);
    m_pass_instances.mesh_finalize_pi.set_storage_buffer(3, m_buffers.chunk_mesh_alloc);
    m_pass_instances.mesh_finalize_pi.set_storage_buffer(4, m_buffers.failed_dirty_list);

    m_pass_instances.mesh_finalize_pi.bind(command_buffer);

    command_buffer.dispatch_indirect(dispatch_args);

    m_buffers.enqueued.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.chunk_meta.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.chunk_mesh_alloc.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.failed_dirty_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::reset_dirty_count(VulkanCommandBuffer& command_buffer) {
    m_pass_instances.reset_dirty_count_pi.set_storage_buffer(0, m_buffers.dirty_list);

    m_pass_instances.reset_dirty_count_pi.set_storage_buffer(1, m_buffers.vb_mesh_allocator_buffers.free_nodes_list);
    m_pass_instances.reset_dirty_count_pi.set_storage_buffer(2, m_buffers.vb_mesh_allocator_buffers.returned_nodes_list);

    m_pass_instances.reset_dirty_count_pi.set_storage_buffer(3, m_buffers.ib_mesh_allocator_buffers.free_nodes_list);
    m_pass_instances.reset_dirty_count_pi.set_storage_buffer(4, m_buffers.ib_mesh_allocator_buffers.returned_nodes_list);

    m_pass_instances.reset_dirty_count_pi.bind(command_buffer);

    command_buffer.dispatch(1, 1, 1);

    m_buffers.dirty_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);

    m_buffers.vb_mesh_allocator_buffers.free_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.vb_mesh_allocator_buffers.returned_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);

    m_buffers.ib_mesh_allocator_buffers.free_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.ib_mesh_allocator_buffers.returned_nodes_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::reset_cmd_count(VulkanCommandBuffer& command_buffer) {
    m_buffers.indirect_cmds.fill(command_buffer, 0u, sizeof(uint32_t), 0u);

    m_buffers.indirect_cmds.memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
    );
}

void VoxelGrid::build_draw_commands(VulkanCommandBuffer& command_buffer, const glm::mat4& view_proj, const glm::vec3& cam_pos, uint32_t pack_bits, int pack_offset) {
    std::array<glm::vec4, 6> planes = math_utils::extract_frustum_planes(view_proj);

    BuildIndirectCmdsUniform unifrom_data {
        .u_chunk_dim = glm::ivec4(m_params.chunk_size, 0),
        .u_voxel_size = glm::vec4(m_params.voxel_size, 0.0f),

        .u_max_chunks = m_params.count_active_chunks,
        .u_ib_page_size_bytes = m_params.ib_allocator_params.page_size,

        .u_pack_bits = pack_bits,
        .u_pack_offset = pack_offset,
        
        .render_distance = m_params.render_distance
    };

    m_buffers.build_indirect_cmds_uniform.upload(&unifrom_data, sizeof(BuildIndirectCmdsUniform));

    m_pass_instances.build_indirect_cmds_pi.set_storage_buffer(0, m_buffers.chunk_meta);
    m_pass_instances.build_indirect_cmds_pi.set_storage_buffer(1, m_buffers.chunk_mesh_alloc);
    m_pass_instances.build_indirect_cmds_pi.set_storage_buffer(2, m_buffers.indirect_cmds);
    m_pass_instances.build_indirect_cmds_pi.set_uniform_buffer(3, m_buffers.build_indirect_cmds_uniform);

    BuildIndirectCmdsPushConstants pc{};
    pc.cam_pos = glm::vec4(cam_pos, 1.0f);
    std::copy(planes.begin(), planes.end(), pc.u_frustum_planes);

    

    m_pass_instances.build_indirect_cmds_pi.bind(command_buffer);

    m_pass_instances.build_indirect_cmds_pi.push_constants(command_buffer, pc);

    uint32_t chunk_groups = math_utils::div_up_u32(m_params.count_active_chunks, 256u);

    command_buffer.dispatch(chunk_groups, 1, 1);

    m_buffers.chunk_mesh_alloc.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.indirect_cmds.memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        VK_ACCESS_INDIRECT_COMMAND_READ_BIT
    );
}

void VoxelGrid::build_mesh_from_dirty(VulkanCommandBuffer& command_buffer, uint32_t pack_bits, int pack_offset) {
    m_shader_helper.prepare_dispatch_args(command_buffer, m_buffers.dispatch_args, BufferDispatchArg(&m_buffers.dirty_list, 0u));
    mesh_reset(command_buffer, m_buffers.dispatch_args);

    m_shader_helper.prepare_dispatch_args(command_buffer, m_buffers.dispatch_args, 
                                          ValueDispatchArg(vox_per_chunk()), BufferDispatchArg(&m_buffers.dirty_list, 0u));
    mesh_count(command_buffer, m_buffers.dispatch_args, pack_bits, pack_offset);

    mesh_alloc(
        command_buffer,
        m_buffers.dispatch_args,
        m_buffers.vb_mesh_allocator_buffers,
        m_params.vb_allocator_params,
        4u * sizeof(VertexGPU),
        true
    );

    mesh_alloc(
        command_buffer,
        m_buffers.dispatch_args,
        m_buffers.ib_mesh_allocator_buffers,
        m_params.ib_allocator_params,
        6u * sizeof(uint32_t),
        false
    );

    m_shader_helper.prepare_dispatch_args(command_buffer, m_buffers.dispatch_args, BufferDispatchArg(&m_buffers.dirty_list, 0u));
    verify_mesh_allocation(command_buffer, m_buffers.dispatch_args);

    prepare_return_free_alloc_nodes(command_buffer, m_buffers.dispatch_args);
    return_free_alloc_nodes(command_buffer, m_buffers.dispatch_args);

    m_shader_helper.prepare_dispatch_args(command_buffer, m_buffers.dispatch_args, ValueDispatchArg(vox_per_chunk()), BufferDispatchArg(&m_buffers.dirty_list, 0u));
    mesh_emit(command_buffer, m_buffers.dispatch_args, pack_bits, pack_offset);

    m_shader_helper.prepare_dispatch_args(command_buffer, m_buffers.dispatch_args, BufferDispatchArg(&m_buffers.dirty_list, 0u));
    mesh_finalize(command_buffer, m_buffers.dispatch_args);

    reset_dirty_count(command_buffer);
}

void VoxelGrid::build_indirect_draw_commands_frustum(VulkanCommandBuffer& command_buffer, 
                                                     const glm::mat4& viewProj, 
                                                     const glm::vec3& cam_pos,
                                                     uint32_t pack_bits,
                                                     int pack_offset) {
    reset_cmd_count(command_buffer);
    build_draw_commands(command_buffer, viewProj, cam_pos, pack_bits, pack_offset);
}

uint64_t VoxelGrid::vox_per_chunk() const noexcept {
    return static_cast<uint64_t>(m_params.chunk_size.x) * 
           static_cast<uint64_t>(m_params.chunk_size.y) * 
           static_cast<uint64_t>(m_params.chunk_size.z);
}

VoxelGrid::VoxelGridParams VoxelGrid::create_params(const VoxelGridDesc& desc) const {
    LOG_METHOD();

    logger().check(desc.chunk_hash_table_size_factor >= 1.0f, "chunk_hash_table_size_factor must be >= 1.0");
    logger().check(desc.car_height_voxels > 0u, "car_height_voxels must be greater than 0");
    logger().check(
        desc.inflation_size <= static_cast<uint32_t>(std::min(desc.chunk_size.x, desc.chunk_size.z)),
        "inflation_size must fit within one chunk in XZ"
    );

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
    params.allocation_retry_list_size = desc.allocation_retry_list_size;
    params.inflation_size = desc.inflation_size;
    params.car_height_voxels = desc.car_height_voxels;
    params.display_inflated_voxels = desc.display_inflated_voxels;
    params.inflated_voxel_color = desc.inflated_voxel_color;

    uint64_t raw = (uint64_t)std::ceil((double)desc.chunk_hash_table_size_factor * (double)desc.count_active_chunks);
    uint32_t base = (raw > UINT32_MAX) ? UINT32_MAX : (uint32_t)raw;
    params.chunk_hash_table_size = math_utils::next_pow2_u32(base);

    logger().check((params.chunk_hash_table_size & (params.chunk_hash_table_size - 1)) == 0, "chunk_hash_table_size must be 2^n");

    // vb_params
    params.vb_allocator_params.page_size = desc.mean_count_quads_in_chunk * 4 * sizeof(VertexGPU);
    params.vb_allocator_params.count_pages = math_utils::next_pow2_u32(
        math_utils::div_up_u32(desc.max_quads * 4u * sizeof(VertexGPU), params.vb_allocator_params.page_size)
    );
    params.vb_allocator_params.count_nodes = ceil(params.vb_allocator_params.count_pages * desc.buddy_allocator_nodes_factor);
    params.vb_allocator_params.max_order = math_utils::log2_floor_u32(params.vb_allocator_params.count_pages);

    // ib_params
    params.ib_allocator_params.page_size = desc.mean_count_quads_in_chunk * 6 * sizeof(uint32_t);
    params.ib_allocator_params.count_pages = math_utils::next_pow2_u32(
        math_utils::div_up_u32(desc.max_quads * 6u * sizeof(uint32_t), params.ib_allocator_params.page_size)
    );
    params.ib_allocator_params.count_nodes = ceil(params.ib_allocator_params.count_pages * desc.buddy_allocator_nodes_factor);
    params.ib_allocator_params.max_order = math_utils::log2_floor_u32(params.ib_allocator_params.count_pages);

    return params;
}

VoxelGrid::VoxelGridPassInstances VoxelGrid::create_pass_instances(VulkanDevice& device, ComputePassManager& compute_pass_manager) const {
    LOG_METHOD();
    
    DescriptorPool& dp = compute_pass_manager.descriptor_pool();

    return VoxelGridPassInstances {
        .fill_buffer_pw = PassWriter(device, compute_pass_manager.fill_buffer_cp),
        .world_init_pi = PassInstance(compute_pass_manager.world_init_cp, dp),
        // .apply_writes_to_world_pi = PassInstance(compute_pass_manager.apply_writes_to_world_cp, dp),
        .mesh_pool_clear_pi = PassInstance(compute_pass_manager.mesh_pool_clear_cp, dp),
        .mesh_pool_seed_pi = PassInstance(compute_pass_manager.mesh_pool_seed_cp, dp),
        .mesh_reset_pi = PassInstance(compute_pass_manager.mesh_reset_cp, dp),
        .mesh_count_pi = PassInstance(compute_pass_manager.mesh_count_cp, dp),
        .mesh_alloc_pw = PassWriter(device, compute_pass_manager.mesh_alloc_cp),
        .retry_mesh_alloc_pw = PassWriter(device, compute_pass_manager.retry_mesh_alloc_cp),
        .verify_mesh_allocation_pi = PassInstance(compute_pass_manager.verify_mesh_allocation_cp, dp),
        .return_free_alloc_nodes_dispatch_adapter_pw = PassWriter(device, compute_pass_manager.return_free_alloc_nodes_dispatch_adapter_cp),
        .return_free_alloc_nodes_pw = PassWriter(device, compute_pass_manager.return_free_alloc_nodes_cp),
        .mesh_emit_pi = PassInstance(compute_pass_manager.mesh_emit_cp, dp),
        .mesh_finalize_pi = PassInstance(compute_pass_manager.mesh_finalize_cp, dp),
        .reset_dirty_count_pi = PassInstance(compute_pass_manager.reset_dirty_count_cp, dp),
        .stream_select_chunks_pi = PassInstance(compute_pass_manager.stream_select_chunks_cp, dp),
        .insert_elements_to_voxel_write_list_pw = PassWriter(device, compute_pass_manager.insert_elements_to_voxel_write_list_cp),
        .add_voxel_write_list_counters_together_pw = PassWriter(device, compute_pass_manager.add_voxel_write_list_counters_together_cp),
        .mark_write_chunks_to_generate_pi = PassInstance(compute_pass_manager.mark_write_chunks_to_generate_cp, dp),
        .stream_generate_terrain_pi = PassInstance(compute_pass_manager.stream_generate_terrain_cp, dp),
        .write_voxels_to_grid_pi = PassInstance(compute_pass_manager.write_voxels_to_grid_cp, dp),
        .evict_buckets_build_pi = PassInstance(compute_pass_manager.evict_buckets_build_cp, dp),
        .evict_low_priority_dispatch_adapter_pw = PassWriter(device, compute_pass_manager.evict_low_priority_dispatch_adapter_cp),
        .evict_low_priority_pi = PassInstance(compute_pass_manager.evict_low_priority_cp, dp),
        .build_indirect_cmds_pi = PassInstance(compute_pass_manager.build_indirect_cmds_cp, dp),
        .free_evicted_chunks_mesh_pi = PassInstance(compute_pass_manager.free_evicted_chunks_mesh_cp, dp),
        .reset_evicted_list_and_buckets_pi = PassInstance(compute_pass_manager.reset_evicted_list_and_buckets_cp, dp),
        .hash_table_conditional_dispatch_adapter_pw = PassWriter(device, compute_pass_manager.hash_table_conditional_dispatch_adapter_cp),
        .clear_chunk_hash_table_pi = PassInstance(compute_pass_manager.clear_chunk_hash_table_cp, dp),
        .fill_chunk_hash_table_pi = PassInstance(compute_pass_manager.fill_chunk_hash_table_cp, dp),
        .read_voxel_grid_chunk_pi = PassInstance(compute_pass_manager.read_voxel_grid_chunk_cp, dp),
        .check_footprint_pi = PassInstance(compute_pass_manager.check_footprint_cp, dp),
        .read_and_inflate_voxel_grid_chunk_pi = PassInstance(compute_pass_manager.read_and_inflate_voxel_grid_chunk_cp, dp),
        .inflate_chunks_pi = PassInstance(compute_pass_manager.inflate_chunks_cp, dp),

        .voxel_writes_from_point_cloud_pi = PassInstance(compute_pass_manager.voxel_writes_from_point_cloud_cp, dp)
    };
}

VoxelGrid::VoxelGridBuffers VoxelGrid::create_buffers(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VulkanCommandBuffer& command_buffer,
    VkMemoryPropertyFlags ssbo_memory_properties) 
{
    LOG_METHOD();
    
    VkDeviceSize free_list_size = sizeof(uint32_t) * (size_t)(1 + m_params.count_active_chunks);
    VkDeviceSize chunk_hash_table_size = sizeof(HashTableCounters) + sizeof(ChunkHashTableSlot) * m_params.chunk_hash_table_size;
    VkDeviceSize mesh_buffers_status_size = sizeof(uint32_t) * 2;
    VkDeviceSize chunk_meta_size = sizeof(ChunkMetaGPU) * (size_t)m_params.count_active_chunks;
    VkDeviceSize enqueued_size = sizeof(uint32_t) * (size_t)m_params.count_active_chunks;
    VkDeviceSize dirty_list_size = sizeof(uint32_t) * (size_t)(1 + m_params.count_active_chunks);
    VkDeviceSize load_list_size = sizeof(uint32_t) * (size_t)(1 + m_params.count_active_chunks);
    VkDeviceSize to_inflate_list_size = sizeof(uint32_t) * (size_t)(1 + m_params.count_active_chunks);
    VkDeviceSize local_voxel_write_list_size = sizeof(uint32_t) * 4 + sizeof(VoxelWriteGPU) * m_params.max_write_count;
    VkDeviceSize voxel_write_list_size = sizeof(uint32_t) * 4 + sizeof(VoxelWriteGPU) * m_params.max_write_count;
    VkDeviceSize voxels_size = sizeof(VoxelDataGPU) * vox_per_chunk() * (size_t)m_params.count_active_chunks;
    VkDeviceSize indirect_cmds_size = sizeof(uint32_t) + sizeof(DrawElementsIndirectCommand) * m_params.count_active_chunks;
    VkDeviceSize failed_dirty_list_size = sizeof(uint32_t) * (size_t)(1 + m_params.count_active_chunks);

    VkDeviceSize bucket_heads_size = sizeof(BucketHead) * m_params.count_evict_buckets;
    VkDeviceSize bucket_next_size = sizeof(uint32_t) * m_params.count_active_chunks;

    VkDeviceSize evicted_chunks_list_size = sizeof(uint32_t) * (m_params.count_active_chunks + 1);
    
    VkDeviceSize global_vertex_buffer_size = m_params.vb_allocator_params.page_size * m_params.vb_allocator_params.count_pages;
    VkDeviceSize global_index_buffer_size = m_params.ib_allocator_params.page_size * m_params.ib_allocator_params.count_pages;

    VkDeviceSize active_splitters_size = sizeof(uint32_t);

    VkDeviceSize vb_heads_size = sizeof(uint32_t) * (size_t)(m_params.vb_allocator_params.max_order + 1);
    VkDeviceSize vb_nodes_size = sizeof(AllocNode) * (size_t)(m_params.vb_allocator_params.count_nodes);
    VkDeviceSize vb_state_size = sizeof(uint32_t) * m_params.vb_allocator_params.count_pages;
    VkDeviceSize vb_free_nodes_list_size = sizeof(uint32_t) * (size_t)(1u + m_params.vb_allocator_params.count_nodes);
    
    VkDeviceSize ib_heads_size = sizeof(uint32_t) * (size_t)(m_params.ib_allocator_params.max_order + 1);
    VkDeviceSize ib_nodes_size = sizeof(AllocNode) * (size_t)(m_params.ib_allocator_params.count_nodes);
    VkDeviceSize ib_state_size = sizeof(uint32_t) * m_params.ib_allocator_params.count_pages;
    VkDeviceSize ib_free_nodes_list_size = sizeof(uint32_t) * (size_t)(1u + m_params.ib_allocator_params.count_nodes);

    VkDeviceSize allocation_retry_list_size = sizeof(uint32_t) + sizeof(uint32_t) * m_params.allocation_retry_list_size; 

    VkDeviceSize chunk_mesh_alloc_size = sizeof(ChunkMeshAlloc) * m_params.count_active_chunks;

    VkDeviceSize dispatch_args_size = sizeof(uint32_t) * 3u;

    VkDeviceSize dirty_quad_count_size = sizeof(uint32_t) * m_params.count_active_chunks;
    VkDeviceSize emit_counters_size = sizeof(uint32_t) * m_params.count_active_chunks;
    VkDeviceSize read_chunk_output_size = sizeof(VoxelDataGPU) * vox_per_chunk();
    VkDeviceSize read_and_inflate_chunk_output_size = read_chunk_output_size * 9u;

    VkDeviceSize chunk_mesh_alloc_local_size = sizeof(ChunkMeshAlloc) * m_params.count_active_chunks;
    VkDeviceSize vb_returned_nodes_list_size = sizeof(uint32_t) * (size_t)(1u + m_params.vb_allocator_params.count_nodes);
    VkDeviceSize ib_returned_nodes_list_size = sizeof(uint32_t) * (size_t)(1u + m_params.ib_allocator_params.count_nodes);

    VulkanBuffer local_voxel_write_list = VulkanBuffer(
        physical_device,
        device,
        local_voxel_write_list_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        ssbo_memory_properties
    );
    
    VulkanBuffer vb_returned_nodes_list = VulkanBuffer(
        physical_device,
        device,
        vb_returned_nodes_list_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        ssbo_memory_properties
    );

    VulkanBuffer ib_returned_nodes_list = VulkanBuffer(
        physical_device,
        device,
        ib_returned_nodes_list_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        ssbo_memory_properties
    );

    VulkanBuffer bucket_heads = VulkanBuffer(
        physical_device,
        device,
        bucket_heads_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        ssbo_memory_properties
    );
    
    VulkanBuffer bucket_next = VulkanBuffer(
        physical_device,
        device,
        bucket_next_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        ssbo_memory_properties
    );

    VulkanBuffer evicted_chunks_list = VulkanBuffer(
        physical_device,
        device,
        evicted_chunks_list_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        ssbo_memory_properties
    );

    VulkanBuffer debug_counter = VulkanBuffer(
        physical_device,
        device,
        sizeof(uint32_t) * 100,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    VulkanBuffer allocation_retry_list = VulkanBuffer(
        physical_device,
        device,
        allocation_retry_list_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        ssbo_memory_properties
    );

    VulkanBuffer allocation_retry_list_additional = VulkanBuffer(
        physical_device,
        device,
        allocation_retry_list_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        ssbo_memory_properties
    );

    {
        auto scope = m_command_buffer.begin_scope();

        local_voxel_write_list.fill(m_command_buffer, 0u);
        vb_returned_nodes_list.fill(m_command_buffer, 0u);
        ib_returned_nodes_list.fill(m_command_buffer, 0u);

        BucketHead bucket_head;
        bucket_head.id = INVALID_ID;
        bucket_head.count = 0;
        bucket_heads.fill(m_command_buffer, m_pass_instances.fill_buffer_pw, bucket_head, bucket_heads_size);

        bucket_next.fill(m_command_buffer, INVALID_ID);
        evicted_chunks_list.fill(m_command_buffer, 0u);
        debug_counter.fill(m_command_buffer, 0u);

        allocation_retry_list.fill(m_command_buffer, 0u, sizeof(uint32_t));
        allocation_retry_list_additional.fill(m_command_buffer, 0u, sizeof(uint32_t));

        // memory_barrier здесь не нужен, так как мы сразу делаем submit...
        // Хотя может быть и нужен :/...
        // На самом деле всё и так вроде работает, но по логике возможно и нужно...
        // Нужно будет поразмыслить над этим...
    }
    submit_compute_commands();
    
    return VoxelGridBuffers {
        .chunk_hash_table = VulkanBuffer(
            physical_device,
            device,
            chunk_hash_table_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            ssbo_memory_properties
        ),
        .free_list = VulkanBuffer(
            physical_device,
            device,
            free_list_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            ssbo_memory_properties
        ),
        .chunk_meta = VulkanBuffer(
            physical_device,
            device,
            chunk_meta_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            ssbo_memory_properties
        ),
        .enqueued = VulkanBuffer(
            physical_device,
            device,
            enqueued_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            ssbo_memory_properties
        ),
        .indirect_cmds = VulkanBuffer(
            physical_device,
            device,
            indirect_cmds_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        ),
        .failed_dirty_list = VulkanBuffer(
            physical_device,
            device,
            failed_dirty_list_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            ssbo_memory_properties
        ),
        .mesh_buffers_status = VulkanBuffer(
            physical_device,
            device,
            mesh_buffers_status_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            ssbo_memory_properties
        ),
        .dirty_list = VulkanBuffer(
            physical_device,
            device,
            dirty_list_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            ssbo_memory_properties
        ),
        .load_list = VulkanBuffer(
            physical_device,
            device,
            load_list_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            ssbo_memory_properties
        ),
        .to_inflate_list = VulkanBuffer(
            physical_device,
            device,
            to_inflate_list_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            ssbo_memory_properties
        ),
        .local_voxel_write_list = std::move(local_voxel_write_list),
        .voxel_write_list = VulkanBuffer(
            physical_device,
            device,
            voxel_write_list_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            ssbo_memory_properties
        ),
        .voxels = VulkanBuffer(
            physical_device,
            device,
            voxels_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            ssbo_memory_properties
        ),
        .bucket_heads = std::move(bucket_heads),
        .bucket_next = std::move(bucket_next),
        .evicted_chunks_list = std::move(evicted_chunks_list),
        .global_vertex_buffer = VulkanBuffer(
            physical_device, 
            device, 
            global_vertex_buffer_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            ssbo_memory_properties
        ),
        .global_index_buffer = VulkanBuffer(
            physical_device, 
            device, 
            global_index_buffer_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            ssbo_memory_properties
        ),
        .active_splitters = VulkanBuffer(
            physical_device, 
            device, 
            active_splitters_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            ssbo_memory_properties
        ),
        .vb_mesh_allocator_buffers = AllocatorBuffers{
            .heads = VulkanBuffer(
                physical_device,
                device,
                vb_heads_size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                ssbo_memory_properties
            ),
            .nodes = VulkanBuffer(
                physical_device,
                device,
                vb_nodes_size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                ssbo_memory_properties
            ),
            .state = VulkanBuffer(
                physical_device,
                device,
                vb_state_size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                ssbo_memory_properties
            ),
            .free_nodes_list = VulkanBuffer(
                physical_device,
                device,
                vb_free_nodes_list_size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                ssbo_memory_properties
            ),
            .returned_nodes_list = std::move(vb_returned_nodes_list)
        },
        .ib_mesh_allocator_buffers = AllocatorBuffers{
            .heads = VulkanBuffer(
                physical_device,
                device,
                ib_heads_size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                ssbo_memory_properties
            ),
            .nodes = VulkanBuffer(
                physical_device,
                device,
                ib_nodes_size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                ssbo_memory_properties
            ),
            .state = VulkanBuffer(
                physical_device,
                device,
                ib_state_size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                ssbo_memory_properties
            ),
            .free_nodes_list = VulkanBuffer(
                physical_device,
                device,
                ib_free_nodes_list_size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                ssbo_memory_properties
            ),
            .returned_nodes_list = std::move(ib_returned_nodes_list)
        },
        .allocation_retry_list = std::move(allocation_retry_list),
        .allocation_retry_list_additional = std::move(allocation_retry_list_additional),
        .chunk_mesh_alloc = VulkanBuffer(
            physical_device,
            device,
            chunk_mesh_alloc_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            ssbo_memory_properties
        ),
        .mesh_pool_clear_uniform = VulkanBuffer(
            physical_device,
            device,
            sizeof(MeshPoolClearUniform),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        ),
        .mesh_pool_seed_uniform = VulkanBuffer(
            physical_device,
            device,
            sizeof(MeshPoolSeedUniform),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        ),
        .dispatch_args = VulkanBuffer(
            physical_device,
            device,
            dispatch_args_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        ),
        .dispatch_args_additional = VulkanBuffer(
            physical_device,
            device,
            dispatch_args_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        ),
        .dirty_quad_count = VulkanBuffer(
            physical_device,
            device,
            dirty_quad_count_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            ssbo_memory_properties
        ),
        .emit_counters = VulkanBuffer(
            physical_device,
            device,
            emit_counters_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            ssbo_memory_properties
        ),
        .chunk_mesh_alloc_local = VulkanBuffer(
            physical_device,
            device,
            chunk_mesh_alloc_local_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            ssbo_memory_properties
        ),
        .build_indirect_cmds_uniform = VulkanBuffer(
            physical_device,
            device,
            sizeof(BuildIndirectCmdsUniform),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        ),

        .read_chunk_output = VulkanBuffer(
            physical_device,
            device,
            read_chunk_output_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        ),
        .check_footprint_result = VulkanBuffer::create_host_visible_storage_buffer(
            physical_device,
            device,
            sizeof(uint32_t)
        ),
        .read_and_inflate_chunk_output = VulkanBuffer(
            physical_device,
            device,
            read_and_inflate_chunk_output_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        ),
        .debug_counter = std::move(debug_counter)
    };
}

void VoxelGrid::insert_elements_to_voxel_write_list(
    VulkanCommandBuffer& command_buffer,
    const VulkanBuffer& dispatch_args,
    const VulkanBuffer& voxel_write_list_src,
    VulkanBuffer& voxel_write_list_dsc)
{
    LOG_METHOD();

    m_pass_instances.insert_elements_to_voxel_write_list_pw.set_storage_buffer(0, voxel_write_list_src);
    m_pass_instances.insert_elements_to_voxel_write_list_pw.set_storage_buffer(1, voxel_write_list_dsc);

    m_pass_instances.insert_elements_to_voxel_write_list_pw.bind(command_buffer);

    command_buffer.dispatch_indirect(dispatch_args);
    voxel_write_list_dsc.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::add_voxel_write_list_counters_together(
    VulkanCommandBuffer& command_buffer,
    const VulkanBuffer& voxel_write_list_src,
    VulkanBuffer& voxel_write_list_dsc)
{
    LOG_METHOD();

    m_pass_instances.add_voxel_write_list_counters_together_pw.set_storage_buffer(0, voxel_write_list_src);
    m_pass_instances.add_voxel_write_list_counters_together_pw.set_storage_buffer(1, voxel_write_list_dsc);

    m_pass_instances.add_voxel_write_list_counters_together_pw.bind(command_buffer);

    command_buffer.dispatch(1, 1, 1);
    voxel_write_list_dsc.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::merge_voxel_write_lists(VulkanCommandBuffer& command_buffer, const VulkanBuffer& voxel_write_list_src, VulkanBuffer& voxel_write_list_dsc) {
    LOG_METHOD();

    m_shader_helper.prepare_dispatch_args(command_buffer, m_buffers.dispatch_args, BufferDispatchArg(&voxel_write_list_src, 0));
    insert_elements_to_voxel_write_list(command_buffer, m_buffers.dispatch_args, voxel_write_list_src, voxel_write_list_dsc);
    add_voxel_write_list_counters_together(command_buffer, voxel_write_list_src, voxel_write_list_dsc);
}

void VoxelGrid::set_voxels(VulkanCommandBuffer& command_buffer, const VulkanBuffer& voxel_write_list_src) {
    LOG_METHOD();
    merge_voxel_write_lists(command_buffer, voxel_write_list_src, m_buffers.local_voxel_write_list);
}

void VoxelGrid::set_render_distance(float value) {
    m_params.render_distance = value;
}

void VoxelGrid::set_inflated_voxel_debug_display(
    uint32_t display_inflated_voxels, 
    uint32_t inflated_voxel_color,
    uint32_t inflation_size) 
{
    std::lock_guard lock(m_compute_mutex);

    if (m_params.display_inflated_voxels == display_inflated_voxels &&
        m_params.inflated_voxel_color == inflated_voxel_color && 
        m_params.inflation_size == inflation_size) {
        return;
    }

    m_params.display_inflated_voxels = display_inflated_voxels;
    m_params.inflated_voxel_color = inflated_voxel_color;
    m_params.inflation_size = inflation_size;

    mark_all_used_chunks_dirty_mesh_cpu();
}

void VoxelGrid::mark_all_used_chunks_dirty_mesh_cpu() {
    logger().check(
        m_buffers.chunk_meta.has_memory_property(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
        m_buffers.enqueued.has_memory_property(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
        m_buffers.dirty_list.has_memory_property(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT),
        "mark_all_used_chunks_dirty_mesh_cpu requires host-visible voxel grid buffers"
    );

    std::vector<ChunkMetaGPU> chunk_meta(m_params.count_active_chunks);
    std::vector<uint32_t> enqueued(m_params.count_active_chunks);

    m_buffers.chunk_meta.read(chunk_meta);
    m_buffers.enqueued.read(enqueued);

    std::vector<uint32_t> dirty_data;
    dirty_data.reserve(size_t(m_params.count_active_chunks) + 1u);
    dirty_data.push_back(0u);

    for (uint32_t chunk_id = 0; chunk_id < m_params.count_active_chunks; ++chunk_id) {
        if (chunk_meta[chunk_id].used != 1u) continue;

        chunk_meta[chunk_id].dirty_flags |= DIRTY_MESH_FLAG_BIT;
        enqueued[chunk_id] |= DIRTY_MESH_FLAG_BIT;
        dirty_data.push_back(chunk_id);
    }

    dirty_data[0] = static_cast<uint32_t>(dirty_data.size() - 1u);

    m_buffers.chunk_meta.upload(chunk_meta);
    m_buffers.enqueued.upload(enqueued);
    m_buffers.dirty_list.upload(dirty_data);
}

VoxelGridChunk VoxelGrid::read_chunk(glm::ivec3 chunk_pos) {
    LOG_METHOD();

    std::lock_guard lock(m_compute_mutex);

    {
        auto scope = m_command_buffer.begin_scope();

        m_pass_instances.read_voxel_grid_chunk_pi.set_storage_buffer(0, m_buffers.chunk_hash_table);
        m_pass_instances.read_voxel_grid_chunk_pi.set_storage_buffer(1, m_buffers.voxels);
        m_pass_instances.read_voxel_grid_chunk_pi.set_storage_buffer(2, m_buffers.read_chunk_output);

        m_pass_instances.read_voxel_grid_chunk_pi.bind(m_command_buffer);

        m_pass_instances.read_voxel_grid_chunk_pi.push_constants(m_command_buffer, ReadVoxelGridChunkPushConstants{
            .u_chunk_dim = glm::ivec4(m_params.chunk_size, 0),
            .chunk_pos = glm::ivec4(chunk_pos, 0),
            .u_chunk_hash_table_size = m_params.chunk_hash_table_size,
            .u_voxels_per_chunk = static_cast<uint32_t>(vox_per_chunk()),
            .u_pack_offset = static_cast<uint32_t>(math_utils::OFFSET),
            .u_pack_bits = math_utils::BITS
        });

        uint32_t groups_x = math_utils::div_up_u32(static_cast<uint32_t>(vox_per_chunk()), 256u);
        m_command_buffer.dispatch(groups_x, 1, 1);

        m_buffers.read_chunk_output.memory_barrier(
            m_command_buffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_ACCESS_HOST_READ_BIT
        );
    }

    submit_compute_commands();

    std::vector<VoxelDataGPU> voxels(static_cast<size_t>(vox_per_chunk()));
    m_buffers.read_chunk_output.read(voxels);

    return VoxelGridChunk(m_params.chunk_size, std::move(voxels));
}

bool VoxelGrid::check_footprint(glm::vec3 origin, glm::vec3 offsets, uint32_t max_step_up) {
    LOG_METHOD();

    logger().check(glm::all(glm::greaterThan(offsets, glm::vec3(0.0f))),
                   "Footprint offsets must be greater than zero");
    logger().check(glm::all(glm::greaterThan(m_params.voxel_size, glm::vec3(0.0f))),
                   "Voxel size must be greater than zero");

    const glm::vec3 voxel_size = m_params.voxel_size;
    const glm::ivec3 voxel_origin = glm::ivec3(glm::floor(origin / voxel_size));
    const glm::ivec3 voxel_end = glm::ivec3(glm::ceil((origin + offsets) / voxel_size));
    const glm::ivec3 voxel_counts_signed = voxel_end - voxel_origin;

    logger().check(glm::all(glm::greaterThan(voxel_counts_signed, glm::ivec3(0))),
                   "Footprint must cover at least one voxel on every axis");

    const glm::uvec3 voxel_counts = glm::uvec3(voxel_counts_signed);
    constexpr uint32_t success = 1u;

    std::lock_guard lock(m_compute_mutex);
    m_buffers.check_footprint_result.upload_scalar(success);

    {
        auto scope = m_command_buffer.begin_scope();

        m_pass_instances.check_footprint_pi.set_storage_buffer(0, m_buffers.chunk_hash_table);
        m_pass_instances.check_footprint_pi.set_storage_buffer(1, m_buffers.voxels);
        m_pass_instances.check_footprint_pi.set_storage_buffer(2, m_buffers.check_footprint_result);
        m_pass_instances.check_footprint_pi.bind(m_command_buffer);

        m_pass_instances.check_footprint_pi.push_constants(m_command_buffer, CheckFootprintPushConstants{
            .u_chunk_dim = glm::ivec4(m_params.chunk_size, 0),
            .u_chunk_hash_table_size = m_params.chunk_hash_table_size,
            .u_voxels_per_chunk = static_cast<uint32_t>(vox_per_chunk()),
            .u_pack_offset = static_cast<uint32_t>(math_utils::OFFSET),
            .u_pack_bits = math_utils::BITS,
            .u_origin = glm::ivec4(voxel_origin, 0),
            .offset_count = glm::uvec4(voxel_counts, 0u),
            .max_step_up = max_step_up
        });

        const uint32_t groups_x = math_utils::div_up_u32(voxel_counts.x, 256u);
        m_command_buffer.dispatch(groups_x, voxel_counts.y, voxel_counts.z);

        m_buffers.check_footprint_result.memory_barrier(
            m_command_buffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_ACCESS_HOST_READ_BIT
        );
    }

    submit_compute_commands();

    uint32_t result = 0u;
    m_buffers.check_footprint_result.read(&result, sizeof(result), 0);
    return result != 0u;
}

std::vector<VoxelGridChunk> VoxelGrid::read_and_inflate_chunk(
    glm::ivec3 chunk_pos,
    uint32_t inflation_size)
{
    LOG_METHOD();

    logger().check(
        inflation_size <= std::min(m_params.chunk_size.x, m_params.chunk_size.z),
        "Inflation size cannot exceed the chunk's X/Z dimensions"
    );

    constexpr uint32_t output_chunk_count = 9u;
    const size_t voxels_per_chunk = static_cast<size_t>(vox_per_chunk());

    std::lock_guard lock(m_compute_mutex);

    {
        auto scope = m_command_buffer.begin_scope();

        m_buffers.read_and_inflate_chunk_output.fill(m_command_buffer, 0u);
        m_buffers.read_and_inflate_chunk_output.memory_barrier(
            m_command_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
        );

        PassInstance& pass = m_pass_instances.read_and_inflate_voxel_grid_chunk_pi;
        pass.set_storage_buffer(0, m_buffers.chunk_hash_table);
        pass.set_storage_buffer(1, m_buffers.voxels);
        pass.set_storage_buffer(2, m_buffers.read_and_inflate_chunk_output);
        pass.bind(m_command_buffer);

        pass.push_constants(m_command_buffer, ReadAndInflateVoxelGridChunkPushConstants{
            .u_chunk_dim = glm::ivec4(m_params.chunk_size, 0),
            .chunk_pos = glm::ivec4(chunk_pos, 0),
            .u_chunk_hash_table_size = m_params.chunk_hash_table_size,
            .u_voxels_per_chunk = static_cast<uint32_t>(voxels_per_chunk),
            .u_pack_offset = static_cast<uint32_t>(math_utils::OFFSET),
            .u_pack_bits = math_utils::BITS,
            .inflation_size = inflation_size
        });

        const uint32_t groups_x = math_utils::div_up_u32(
            output_chunk_count * static_cast<uint32_t>(voxels_per_chunk),
            256u
        );
        m_command_buffer.dispatch(groups_x, 1, 1);

        m_buffers.read_and_inflate_chunk_output.memory_barrier(
            m_command_buffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_ACCESS_HOST_READ_BIT
        );
    }

    submit_compute_commands();

    std::vector<VoxelDataGPU> output_voxels(output_chunk_count * voxels_per_chunk);
    m_buffers.read_and_inflate_chunk_output.read(output_voxels);

    std::vector<VoxelGridChunk> chunks;
    chunks.reserve(output_chunk_count);

    for (uint32_t chunk_id = 0; chunk_id < output_chunk_count; ++chunk_id) {
        auto begin = output_voxels.begin() + chunk_id * voxels_per_chunk;
        auto end = begin + voxels_per_chunk;
        chunks.emplace_back(
            m_params.chunk_size,
            std::vector<VoxelDataGPU>(begin, end)
        );
    }

    return chunks;
}

glm::ivec3 VoxelGrid::chunk_pos_from_voxel_pos(glm::ivec3 voxel_pos) {
    return glm::ivec3(
        math_utils::floor_div(voxel_pos.x, static_cast<int>(m_params.chunk_size.x)),
        math_utils::floor_div(voxel_pos.y, static_cast<int>(m_params.chunk_size.y)),
        math_utils::floor_div(voxel_pos.z, static_cast<int>(m_params.chunk_size.z))
    );
}

glm::ivec3 VoxelGrid::pos_in_chunk_from_global_voxel_pos(glm::ivec3 voxel_pos) {
    glm::ivec3 chunk_pos = chunk_pos_from_voxel_pos(voxel_pos);
    return voxel_pos - chunk_pos * glm::ivec3(m_params.chunk_size);
}

glm::ivec3 VoxelGrid::pos_in_chunk_from_global_voxel_pos(glm::ivec3 chunk_pos, glm::ivec3 voxel_pos) {
    return voxel_pos - chunk_pos * glm::ivec3(m_params.chunk_size);
}

void VoxelGrid::add_next_to_stream_chunks_sphere_callback(
    std::function<void(VulkanCommandBuffer&, VoxelGrid&)> callback) {
    m_next_to_stream_chunks_sphere_callbacks.push_back(callback);
}

void VoxelGrid::add_next_to_update_submit_callbacks(std::function<void(VoxelGrid&)> callback) {
    m_next_to_update_submit_callbacks.push_back(callback);
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
        m_pass_instances.world_init_pi.set_storage_buffer(9, m_buffers.to_inflate_list);

        m_pass_instances.world_init_pi.bind(m_command_buffer);

        m_pass_instances.world_init_pi.push_constants(m_command_buffer, WorldInitPushConstants{
            .u_chunk_hash_table_size = m_params.chunk_hash_table_size,
            .u_max_chunks = m_params.count_active_chunks
        });

        uint32_t max_items = std::max(m_params.chunk_hash_table_size, m_params.count_active_chunks);
        uint32_t groups_x = math_utils::div_up_u32(max_items, 256u);
        m_command_buffer.dispatch(groups_x, 1, 1);

        m_buffers.chunk_hash_table.memory_barrier_compute_write_to_compute_write_read(m_command_buffer);
        m_buffers.free_list.memory_barrier_compute_write_to_compute_write_read(m_command_buffer);
    }

    submit_compute_commands();
}

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
            .u_vb_pages = m_params.vb_allocator_params.count_pages,
            .u_ib_pages = m_params.ib_allocator_params.count_pages,
            .u_vb_nodes = m_params.vb_allocator_params.count_nodes,
            .u_ib_nodes = m_params.ib_allocator_params.count_nodes,
            .u_vb_heads_count = m_params.vb_allocator_params.max_order + 1,
            .u_ib_heads_count = m_params.ib_allocator_params.max_order + 1,
            .u_max_chunks = m_params.count_active_chunks
        };

        m_buffers.mesh_pool_clear_uniform.upload(&mesh_pool_clear_uniform, sizeof(MeshPoolClearUniform));
        
        m_pass_instances.mesh_pool_clear_pi.set_storage_buffer(0, m_buffers.vb_mesh_allocator_buffers.heads);
        m_pass_instances.mesh_pool_clear_pi.set_storage_buffer(1, m_buffers.vb_mesh_allocator_buffers.state);
        m_pass_instances.mesh_pool_clear_pi.set_storage_buffer(2, m_buffers.vb_mesh_allocator_buffers.free_nodes_list);
        m_pass_instances.mesh_pool_clear_pi.set_storage_buffer(3, m_buffers.ib_mesh_allocator_buffers.heads);
        m_pass_instances.mesh_pool_clear_pi.set_storage_buffer(4, m_buffers.ib_mesh_allocator_buffers.state);
        m_pass_instances.mesh_pool_clear_pi.set_storage_buffer(5, m_buffers.ib_mesh_allocator_buffers.free_nodes_list);
        m_pass_instances.mesh_pool_clear_pi.set_storage_buffer(6, m_buffers.chunk_mesh_alloc);
        m_pass_instances.mesh_pool_clear_pi.set_uniform_buffer(7, m_buffers.mesh_pool_clear_uniform);

        m_pass_instances.mesh_pool_clear_pi.bind(m_command_buffer);

        uint32_t max_count = std::max({m_params.vb_allocator_params.count_pages, m_params.ib_allocator_params.count_pages, m_params.count_active_chunks, 
                                       m_params.vb_allocator_params.count_nodes, m_params.ib_allocator_params.count_nodes});
        uint32_t groups_x = math_utils::div_up_u32(max_count, 256u);

        m_command_buffer.dispatch(groups_x, 1, 1);

        compute_write_to_compute_read_write(m_buffers.vb_mesh_allocator_buffers.heads);
        compute_write_to_compute_read_write(m_buffers.vb_mesh_allocator_buffers.state);
        compute_write_to_compute_read_write(m_buffers.vb_mesh_allocator_buffers.free_nodes_list);

        compute_write_to_compute_read_write(m_buffers.ib_mesh_allocator_buffers.heads);
        compute_write_to_compute_read_write(m_buffers.ib_mesh_allocator_buffers.state);
        compute_write_to_compute_read_write(m_buffers.ib_mesh_allocator_buffers.free_nodes_list);

        compute_write_to_compute_read_write(m_buffers.chunk_mesh_alloc);


        // Pass 2: mesh_pool_seed
        MeshPoolSeedUniform mesh_pool_seed_uniform {
            .u_vb_max_order = m_params.vb_allocator_params.max_order,
            .u_ib_max_order = m_params.ib_allocator_params.max_order
        };

        m_buffers.mesh_pool_seed_uniform.upload(&mesh_pool_seed_uniform, sizeof(MeshPoolSeedUniform));

        m_pass_instances.mesh_pool_seed_pi.set_storage_buffer(0, m_buffers.vb_mesh_allocator_buffers.heads);
        m_pass_instances.mesh_pool_seed_pi.set_storage_buffer(1, m_buffers.vb_mesh_allocator_buffers.nodes);
        m_pass_instances.mesh_pool_seed_pi.set_storage_buffer(2, m_buffers.vb_mesh_allocator_buffers.state);
        m_pass_instances.mesh_pool_seed_pi.set_storage_buffer(3, m_buffers.vb_mesh_allocator_buffers.free_nodes_list);
        m_pass_instances.mesh_pool_seed_pi.set_storage_buffer(4, m_buffers.vb_mesh_allocator_buffers.returned_nodes_list);
        m_pass_instances.mesh_pool_seed_pi.set_storage_buffer(5, m_buffers.ib_mesh_allocator_buffers.heads);
        m_pass_instances.mesh_pool_seed_pi.set_storage_buffer(6, m_buffers.ib_mesh_allocator_buffers.nodes);
        m_pass_instances.mesh_pool_seed_pi.set_storage_buffer(7, m_buffers.ib_mesh_allocator_buffers.state);
        m_pass_instances.mesh_pool_seed_pi.set_storage_buffer(8, m_buffers.ib_mesh_allocator_buffers.free_nodes_list);
        m_pass_instances.mesh_pool_seed_pi.set_storage_buffer(9, m_buffers.ib_mesh_allocator_buffers.returned_nodes_list);
        m_pass_instances.mesh_pool_seed_pi.set_uniform_buffer(10, m_buffers.mesh_pool_seed_uniform);

        m_pass_instances.mesh_pool_seed_pi.bind(m_command_buffer);

        m_command_buffer.dispatch(1, 1, 1);

        compute_write_to_compute_read_write(m_buffers.vb_mesh_allocator_buffers.heads);
        compute_write_to_compute_read_write(m_buffers.vb_mesh_allocator_buffers.nodes);
        compute_write_to_compute_read_write(m_buffers.vb_mesh_allocator_buffers.state);
        compute_write_to_compute_read_write(m_buffers.vb_mesh_allocator_buffers.free_nodes_list);

        compute_write_to_compute_read_write(m_buffers.ib_mesh_allocator_buffers.heads);
        compute_write_to_compute_read_write(m_buffers.ib_mesh_allocator_buffers.nodes);
        compute_write_to_compute_read_write(m_buffers.ib_mesh_allocator_buffers.state);
        compute_write_to_compute_read_write(m_buffers.ib_mesh_allocator_buffers.free_nodes_list);
    }

    submit_compute_commands();
}

IndirectRenderObject& VoxelGrid::render_object() {
    return m_render_object;
}

VulkanBuffer& VoxelGrid::local_voxel_write_list() noexcept {
    return m_buffers.local_voxel_write_list;
}

const VoxelGrid::VoxelGridParams& VoxelGrid::params() const noexcept{
    return m_params;
}

VoxelGrid::VoxelGridBuffers& VoxelGrid::buffers() noexcept {
    return m_buffers;
}

ShaderHelper& VoxelGrid::shader_helper() noexcept {
    return m_shader_helper;
}

glm::vec3 VoxelGrid::voxel_size() noexcept {
    return m_params.voxel_size;
}

void VoxelGrid::voxelize_point_cloud(VulkanCommandBuffer& command_buffer, VulkanEngine& engine, 
                                     PointCloud& point_cloud, VulkanBuffer& voxel_writes, uint32_t max_write_count) {
    LOG_METHOD();

    logger().check(point_cloud.point_count() < max_write_count, "Point count was greater than max write count");
    logger().check(point_cloud.point_count() < m_params.max_write_count, "Point count was greater than max write count");

    m_pass_instances.voxel_writes_from_point_cloud_pi.set_storage_buffer(0, point_cloud.instance_buffer());
    m_pass_instances.voxel_writes_from_point_cloud_pi.set_storage_buffer(1, voxel_writes);

    m_pass_instances.voxel_writes_from_point_cloud_pi.push_constants(command_buffer, 
    VoxelListFromPointCloudPushConstants{
        .source_point_cloud_model = point_cloud.transform.get_model_matrix(),
        .voxel_size = glm::vec4(m_params.voxel_size, 1.0f),
        .point_count = point_cloud.point_count(),
        .max_write_count = max_write_count
    });
    
    m_pass_instances.voxel_writes_from_point_cloud_pi.bind(command_buffer);

    uint32_t x_groups = math_utils::div_up_u32(point_cloud.point_count(), 256);

    command_buffer.dispatch(x_groups, 1, 1);

    voxel_writes.memory_barrier_compute_write_to_compute_write_read(command_buffer);

    set_voxels(command_buffer, voxel_writes);
}

void VoxelGrid::voxelize_point_cloud(VulkanEngine& engine, PointCloud& point_cloud, 
                                     VulkanBuffer& voxel_writes, uint32_t max_write_count) {
    LOG_METHOD();
    std::lock_guard lock(m_compute_mutex);
    {
        auto scope = m_command_buffer.begin_scope();
        voxelize_point_cloud(m_command_buffer, engine, point_cloud, voxel_writes, max_write_count);
    }
    submit_compute_commands();
}

void VoxelGrid::update(Window& window, Camera& camera) {
    float aspect = float(window.width()) / float(window.height());
    glm::mat4 vp = camera.get_projection_matrix(aspect) * camera.get_view_matrix();

    std::lock_guard lock(m_compute_mutex);

    {
        auto scope = m_command_buffer.begin_scope();
        stream_chunks_sphere(m_command_buffer, camera.position, -1, 42);

        // inflate_marked_chunks(m_command_buffer);

        for (auto& callback : m_next_to_stream_chunks_sphere_callbacks) {
            callback(m_command_buffer, *this);
        }

        build_mesh_from_dirty(m_command_buffer, math_utils::BITS, math_utils::OFFSET);
        build_indirect_draw_commands_frustum(m_command_buffer, vp, camera.position, math_utils::BITS, math_utils::OFFSET);
    }
    
    submit_compute_commands();

    for (auto& callback : m_next_to_update_submit_callbacks) {
        callback(*this);
    }
}

void VoxelGrid::submit_compute_commands() {
    LOG_METHOD();

    logger().check(m_queue != nullptr, "VoxelGrid queue was not initialized");

    m_fence.reset();
    m_queue->submit(m_command_buffer, &m_fence);
    m_fence.wait();
    m_command_buffer.reset();
}

void VoxelGrid::reset_load_list_counter(VulkanCommandBuffer& command_buffer) {
    LOG_METHOD();
    m_buffers.load_list.fill(command_buffer, 0, sizeof(uint32_t));
    m_buffers.load_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::reset_to_inflate_list_counter(VulkanCommandBuffer& command_buffer) {
    LOG_METHOD();
    m_buffers.to_inflate_list.fill(command_buffer, 0, sizeof(uint32_t));
    m_buffers.to_inflate_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::mark_chunk_to_generate(VulkanCommandBuffer& command_buffer, glm::vec3 cam_world_pos, int radius_chunks) {
    LOG_METHOD();

    // glm::mat4 invM = glm::inverse(get_model_matrix());
    glm::mat4 invM = glm::identity<glm::mat4>(); // #TODO
    glm::vec3 cam_local = glm::vec3(invM * glm::vec4(cam_world_pos, 1.0f));

    m_pass_instances.stream_select_chunks_pi.set_storage_buffer(0, m_buffers.chunk_hash_table);
    m_pass_instances.stream_select_chunks_pi.set_storage_buffer(1, m_buffers.free_list);
    m_pass_instances.stream_select_chunks_pi.set_storage_buffer(2, m_buffers.chunk_meta);
    m_pass_instances.stream_select_chunks_pi.set_storage_buffer(3, m_buffers.enqueued);
    m_pass_instances.stream_select_chunks_pi.set_storage_buffer(4, m_buffers.load_list);

    m_pass_instances.stream_select_chunks_pi.bind(command_buffer);

    m_pass_instances.stream_select_chunks_pi.push_constants(command_buffer, StreamSelectChunksPushConstants{
        .u_chunk_hash_table_size = m_params.chunk_hash_table_size,
        .u_max_load_entries = m_params.count_active_chunks,
        .u_chunk_dim = glm::ivec4(m_params.chunk_size, 0),
        .u_voxel_size = glm::vec4(m_params.voxel_size, 0.0f),
        .u_cam_pos_local = glm::vec4(cam_local, 0),
        .u_radius_chunks = radius_chunks,
        .u_pack_bits = math_utils::BITS,
        .u_pack_offset =  math_utils::OFFSET
    });

    const uint32_t side = (uint32_t)(2 * radius_chunks + 1);
    const uint32_t gx = math_utils::div_up_u32(side, 8u);
    const uint32_t gy = math_utils::div_up_u32(side, 8u);
    const uint32_t gz = math_utils::div_up_u32(side, 8u);

    command_buffer.dispatch(gx, gy, gz);

    m_buffers.chunk_hash_table.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.free_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.chunk_meta.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.enqueued.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.load_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::mark_write_chunks_to_generate(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args) {
    LOG_METHOD();

    m_pass_instances.mark_write_chunks_to_generate_pi.set_storage_buffer(0, m_buffers.voxel_write_list);
    m_pass_instances.mark_write_chunks_to_generate_pi.set_storage_buffer(1, m_buffers.load_list);
    m_pass_instances.mark_write_chunks_to_generate_pi.set_storage_buffer(2, m_buffers.chunk_hash_table);
    m_pass_instances.mark_write_chunks_to_generate_pi.set_storage_buffer(3, m_buffers.free_list);
    m_pass_instances.mark_write_chunks_to_generate_pi.set_storage_buffer(4, m_buffers.chunk_meta);

    m_pass_instances.mark_write_chunks_to_generate_pi.bind(command_buffer);

    m_pass_instances.mark_write_chunks_to_generate_pi.push_constants(command_buffer, MarkWriteChunksToGeneratePushConstants{
        .u_chunk_dim = glm::uvec4{m_params.chunk_size.x, m_params.chunk_size.y, m_params.chunk_size.z, 0},
        .u_chunk_hash_table_size = m_params.chunk_hash_table_size,
        .u_pack_offset = math_utils::OFFSET,
        .u_pack_bits = math_utils::BITS
    });

    command_buffer.dispatch_indirect(dispatch_args);

    m_buffers.load_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.chunk_hash_table.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.free_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.chunk_meta.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::generate_terrain(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args, uint32_t seed) {
    LOG_METHOD();

    m_pass_instances.stream_generate_terrain_pi.set_storage_buffer(0, m_buffers.chunk_hash_table);
    m_pass_instances.stream_generate_terrain_pi.set_storage_buffer(1, m_buffers.load_list);
    m_pass_instances.stream_generate_terrain_pi.set_storage_buffer(2, m_buffers.voxels);
    m_pass_instances.stream_generate_terrain_pi.set_storage_buffer(3, m_buffers.chunk_meta);
    m_pass_instances.stream_generate_terrain_pi.set_storage_buffer(4, m_buffers.enqueued);
    m_pass_instances.stream_generate_terrain_pi.set_storage_buffer(5, m_buffers.dirty_list);
    m_pass_instances.stream_generate_terrain_pi.set_storage_buffer(6, m_buffers.to_inflate_list);


    // StreamGenerateTerrainPushConstants stream_sdhflsd{
    //     .u_chunk_dim = glm::ivec4(m_params.chunk_size.x, m_params.chunk_size.y, m_params.chunk_size.z, 0), 
    //     .u_voxels_per_chunk = static_cast<uint32_t>(vox_per_chunk()),
    //     .u_pack_bits = math_utils::BITS,
    //     .u_pack_offset = math_utils::OFFSET,
    //     .u_seed = seed,
    //     .u_chunk_hash_table_size = m_params.chunk_hash_table_size
    // };


    m_pass_instances.stream_generate_terrain_pi.push_constants(command_buffer, StreamGenerateTerrainPushConstants{
        .u_chunk_dim = glm::ivec4(m_params.chunk_size.x, m_params.chunk_size.y, m_params.chunk_size.z, 0), 
        .u_voxels_per_chunk = static_cast<uint32_t>(vox_per_chunk()),
        .u_pack_bits = math_utils::BITS,
        .u_pack_offset = math_utils::OFFSET,
        .u_seed = seed,
        .u_chunk_hash_table_size = m_params.chunk_hash_table_size
    });

    // m_pass_instances.stream_generate_terrain_pi.push_constants(command_buffer, stream_sdhflsd);
    
    
    m_pass_instances.stream_generate_terrain_pi.bind(command_buffer);

    

    command_buffer.dispatch_indirect(dispatch_args);

    m_buffers.chunk_hash_table.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.voxels.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.chunk_meta.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.enqueued.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.dirty_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.to_inflate_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::write_voxels_to_grid(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args) {
    LOG_METHOD();

    m_pass_instances.write_voxels_to_grid_pi.set_storage_buffer(0, m_buffers.chunk_hash_table);
    m_pass_instances.write_voxels_to_grid_pi.set_storage_buffer(1, m_buffers.free_list);
    m_pass_instances.write_voxels_to_grid_pi.set_storage_buffer(2, m_buffers.chunk_meta);
    m_pass_instances.write_voxels_to_grid_pi.set_storage_buffer(3, m_buffers.enqueued);
    m_pass_instances.write_voxels_to_grid_pi.set_storage_buffer(4, m_buffers.dirty_list);
    m_pass_instances.write_voxels_to_grid_pi.set_storage_buffer(5, m_buffers.voxel_write_list);
    m_pass_instances.write_voxels_to_grid_pi.set_storage_buffer(6, m_buffers.voxels);
    m_pass_instances.write_voxels_to_grid_pi.set_storage_buffer(7, m_buffers.to_inflate_list);
    
    m_pass_instances.write_voxels_to_grid_pi.bind(command_buffer);

    m_pass_instances.write_voxels_to_grid_pi.push_constants(command_buffer, WriteVoxelsToGridPushConstants{
        .u_chunk_dim = glm::ivec4(m_params.chunk_size.x, m_params.chunk_size.y, m_params.chunk_size.z, 0),
        .u_chunk_hash_table_size = m_params.chunk_hash_table_size,
        .u_voxels_per_chunk = static_cast<uint32_t>(vox_per_chunk()),

        .u_pack_offset = math_utils::OFFSET,
        .u_pack_bits = math_utils::BITS
    });

    command_buffer.dispatch_indirect(dispatch_args);

    m_buffers.chunk_hash_table.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.free_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.chunk_meta.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.enqueued.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.dirty_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.to_inflate_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.voxels.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::reset_voxel_write_list_counter(VulkanCommandBuffer& command_buffer, VulkanBuffer& voxel_write_list) {
    LOG_METHOD();
    
    voxel_write_list.fill(command_buffer, 0u, sizeof(uint32_t));
    voxel_write_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void VoxelGrid::stream_chunks_sphere(VulkanCommandBuffer& command_buffer, glm::vec3 cam_world_pos, int radius_chunks, uint32_t seed) {
    LOG_METHOD();

    if (radius_chunks < 0) radius_chunks = m_params.generation_distance;

    // ensure_free_chunks_gpu(command_buffer, cam_world_pos, math_utils::BITS, math_utils::OFFSET);

    reset_load_list_counter(command_buffer);

    mark_chunk_to_generate(command_buffer, cam_world_pos, radius_chunks);

    merge_voxel_write_lists(command_buffer, m_buffers.local_voxel_write_list, m_buffers.voxel_write_list);

    reset_voxel_write_list_counter(command_buffer, m_buffers.local_voxel_write_list);
    
    m_shader_helper.prepare_dispatch_args(command_buffer, m_buffers.dispatch_args, BufferDispatchArg(&m_buffers.voxel_write_list, 0));
    mark_write_chunks_to_generate(command_buffer, m_buffers.dispatch_args);

    m_shader_helper.prepare_dispatch_args(command_buffer, m_buffers.dispatch_args, ValueDispatchArg(vox_per_chunk()), BufferDispatchArg(&m_buffers.load_list, 0u));
    generate_terrain(command_buffer, m_buffers.dispatch_args, seed);

    m_shader_helper.prepare_dispatch_args(command_buffer, m_buffers.dispatch_args, BufferDispatchArg(&m_buffers.voxel_write_list, 0u));
    write_voxels_to_grid(command_buffer, m_buffers.dispatch_args);

    reset_voxel_write_list_counter(command_buffer, m_buffers.voxel_write_list);
}

void VoxelGrid::inflate_chunks(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_arg) {
    LOG_METHOD();

    m_pass_instances.inflate_chunks_pi.set_storage_buffer(0, m_buffers.chunk_hash_table);
    m_pass_instances.inflate_chunks_pi.set_storage_buffer(1, m_buffers.voxels);
    m_pass_instances.inflate_chunks_pi.set_storage_buffer(2, m_buffers.to_inflate_list);
    m_pass_instances.inflate_chunks_pi.set_storage_buffer(3, m_buffers.chunk_meta);
    m_pass_instances.inflate_chunks_pi.set_storage_buffer(4, m_buffers.enqueued);

    m_pass_instances.inflate_chunks_pi.bind(command_buffer);

    m_pass_instances.inflate_chunks_pi.push_constants(command_buffer, InflateChunksPushConstants{
        .u_chunk_dim = glm::ivec4(m_params.chunk_size, 0),
        .u_chunk_hash_table_size = m_params.chunk_hash_table_size,
        .u_voxels_per_chunk = static_cast<uint32_t>(vox_per_chunk()),
        .u_pack_offset = static_cast<uint32_t>(math_utils::OFFSET),
        .u_pack_bits = math_utils::BITS,
        .u_inflation_size = m_params.inflation_size,
        .u_car_height_voxels = m_params.car_height_voxels
    });

    command_buffer.dispatch_indirect(dispatch_arg);

    m_buffers.voxels.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.chunk_meta.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.enqueued.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.to_inflate_list.memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT
    );
}

void VoxelGrid::inflate_marked_chunks(VulkanCommandBuffer& command_buffer) {
    LOG_METHOD();

    m_shader_helper.prepare_dispatch_args(
        command_buffer, 
        m_buffers.dispatch_args, 
        ValueDispatchArg(static_cast<uint32_t>(vox_per_chunk())),
        BufferDispatchArg(&m_buffers.to_inflate_list, 0u)
    );
    inflate_chunks(command_buffer, m_buffers.dispatch_args);

    // Очистка идёт в конце
    reset_to_inflate_list_counter(command_buffer);
}
