#include "voxelizator.h"

#include <utility>

#include "../vulkan_self/vulkan_physical_device.h"
#include "../vulkan_self/vulkan_device.h"
#include "../vulkan_self/vulkan_queue.h"
#include "../managers/compute_pass_manager.h"
#include "../vulkan_self/descriptor_set/descriptor_pool.h"
#include "../renderer/mesh_view.h"
#include "shader_helper/buffer_dispatch_arg.h"
#include "shader_helper/value_dispatch_arg.h"
#include "../vulkan_self/push_constants_structures.h"
#include "../math_utils.h"

Voxelizator::Voxelizator(
    const VulkanPhysicalDevice& physical_device,
    VulkanDevice& device,
    VulkanQueue& queue,
    ComputePassManager& compute_pass_manager,
    const VoxelizatorDesc& desc)
    :   m_command_pool(device, queue),
        m_command_buffer(device, m_command_pool),
        m_fence(device),
        m_queue(&queue),
        m_params(desc),
        m_buffers(create_buffers(physical_device, device, m_command_buffer)),
        m_pass_instances(create_pass_instances(device, compute_pass_manager)),
        m_shader_helper(device, compute_pass_manager) {}

Voxelizator::VoxelizatorBuffers Voxelizator::create_buffers(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VulkanCommandBuffer& command_buffer)
{
    LOG_METHOD();

    VkDeviceSize dispatch_args_size = sizeof(uint32_t) * 3;
    VkDeviceSize counter_hash_table_size = sizeof(HashTableCounters) + sizeof(CounterHashTableSlot) * m_params.counter_hash_table_size;
    VkDeviceSize active_chunk_keys_list_size = sizeof(uint64_t) * (m_params.counter_hash_table_size + 1u);
    VkDeviceSize triangle_indices_list_size = sizeof(uint32_t) * (m_params.counter_hash_table_size + 1u);
    VkDeviceSize voxel_writes_size = sizeof(uint32_t) * 4 + sizeof(VoxelWriteGPU) * m_params.count_voxel_writes;
    VkDeviceSize counter_hash_table_failure_slots_size = sizeof(uint32_t) * 2 + sizeof(uint64_t) * m_params.count_hash_table_failure_slots;

    VulkanBuffer triangle_indices_list = VulkanBuffer(
        physical_device,
        device,
        triangle_indices_list_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    {
        auto scope = m_command_buffer.begin_scope();
        triangle_indices_list.fill(m_command_buffer, 0u);

        // memory_barrier здесь не нужен, так как мы сразу делаем submit
    }
    submit_compute_commands();

    return VoxelizatorBuffers{
        .dispatch_args = VulkanBuffer::create_host_visible_indirect_storage_buffer(physical_device, device, dispatch_args_size),
        .counter_hash_table = VulkanBuffer::create_storage_buffer(physical_device, device, counter_hash_table_size),
        .counter_hash_table_failure_slots = VulkanBuffer(
            physical_device,
            device,
            counter_hash_table_failure_slots_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        ),
        .counter_hash_table_failure_slots_additional = VulkanBuffer(
            physical_device,
            device,
            counter_hash_table_failure_slots_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        ),
        .active_chunk_keys_list = VulkanBuffer::create_storage_buffer(physical_device, device, active_chunk_keys_list_size),
        .triangle_indices_list = std::move(triangle_indices_list),
        .voxel_writes = VulkanBuffer::create_storage_buffer(physical_device, device, voxel_writes_size),
    };
}

Voxelizator::VoxelizatorPassInstances Voxelizator::create_pass_instances(
    VulkanDevice& device, 
    ComputePassManager& compute_pass_manager) const 
{
    LOG_METHOD();

    DescriptorPool& dp = compute_pass_manager.descriptor_pool();

    return VoxelizatorPassInstances{
        .reset_voxelize_pipeline_pi = PassInstance(compute_pass_manager.reset_voxelize_pipeline_cp, dp),
        .mark_and_count_active_chunks_pi = PassInstance(compute_pass_manager.mark_and_count_active_chunks_cp, dp),
        .mark_and_count_fail_slots_pw = PassWriter(device, compute_pass_manager.mark_and_count_fail_slots_cp),
        .alloc_active_chunk_triangles_pi = PassInstance(compute_pass_manager.alloc_active_chunk_triangles_cp, dp),
        .fill_triangle_indices_pi = PassInstance(compute_pass_manager.fill_triangle_indices_cp, dp),
        .voxelize_triangles_pi = PassInstance(compute_pass_manager.voxelize_triangles_cp, dp)
    };
}

void Voxelizator::submit_compute_commands() {
    LOG_METHOD();

    logger.check(m_queue != nullptr, "VoxelGrid queue was not initialized");

    m_fence.reset();
    m_queue->submit(m_command_buffer, &m_fence);
    m_fence.wait();
    m_command_buffer.reset();
}

void Voxelizator::voxelize(
    VulkanCommandBuffer& command_buffer,
    const VoxelWriteGPU& prifab,
    MeshView mesh,
    uint32_t position_attribute_offset,
    uint32_t vertex_stride,
    glm::mat4 transform,
    VulkanBuffer* out_voxel_writes)
{
    LOG_METHOD();
    VulkanBuffer* voxel_writes_buffer = out_voxel_writes ? out_voxel_writes : &m_buffers.voxel_writes;

    reset_voxelize_pipline(command_buffer, *voxel_writes_buffer, out_voxel_writes == nullptr);
    
    reset_failure_slots_counter(command_buffer, m_buffers.counter_hash_table_failure_slots);
    mark_and_count_active_chunks(command_buffer, mesh, position_attribute_offset, vertex_stride, transform);

    VulkanBuffer* readable_failure_slots_buffer = &m_buffers.counter_hash_table_failure_slots;
    VulkanBuffer* writable_failure_slots_buffer = &m_buffers.counter_hash_table_failure_slots_additional;
    for (uint32_t attempt = 0; attempt < m_params.count_hash_table_attempts; attempt++) {
        reset_failure_slots_counter(command_buffer, *writable_failure_slots_buffer);
        m_shader_helper.prepare_dispatch_args(
            command_buffer,
            m_buffers.dispatch_args,
            BufferDispatchArg(readable_failure_slots_buffer, 0)
        );    
        mark_and_count_fail_slots(
            command_buffer,
            m_buffers.dispatch_args,
            *readable_failure_slots_buffer,
            *writable_failure_slots_buffer
        );
        std::swap(readable_failure_slots_buffer, writable_failure_slots_buffer);
    }

    m_shader_helper.prepare_dispatch_args(
        command_buffer,
        m_buffers.dispatch_args,
        BufferDispatchArg(&m_buffers.active_chunk_keys_list, 0)
    );
    alloc_active_chunk_triangles(command_buffer, m_buffers.dispatch_args);

    fill_triangle_indices(command_buffer, mesh, position_attribute_offset, vertex_stride, transform);

    m_shader_helper.prepare_dispatch_args(
        command_buffer,
        m_buffers.dispatch_args, 
        ValueDispatchArg(m_params.chunk_size.x * m_params.chunk_size.y * m_params.chunk_size.z), 
        BufferDispatchArg(&m_buffers.active_chunk_keys_list, 0)
    );
    voxelize_chunks(
        command_buffer,
        m_buffers.dispatch_args,
        *voxel_writes_buffer,
        prifab.voxel_data.type_flags,
        prifab.voxel_data.color,
        prifab.set_flags,
        mesh,
        position_attribute_offset,
        vertex_stride,
        transform
    );

    if (out_voxel_writes == nullptr) {
        logger.throw_error("Not implemented yet :D");
        // if (gridable_gpu != nullptr) {
        //     gridable_gpu->set_voxels(voxel_writes_);
        // } else {
        //     //TODO
        // }
    }
}

void Voxelizator::voxelize_and_submit(
    const VoxelWriteGPU& prifab,
    MeshView mesh,
    uint32_t position_attribute_offset,
    uint32_t vertex_stride,
    glm::mat4 transform,
    VulkanBuffer* out_voxel_writes)
{
    LOG_METHOD();

    {
        auto scope = m_command_buffer.begin_scope();
        voxelize(
            m_command_buffer,
            prifab,
            mesh,
            position_attribute_offset,
            vertex_stride,
            transform,
            out_voxel_writes
        );
    }

    submit_compute_commands();
}

void Voxelizator::reset_voxelize_pipline(VulkanCommandBuffer& command_buffer, VulkanBuffer& voxel_writes, bool reset_voxel_write_list) {
    LOG_METHOD();

    m_pass_instances.reset_voxelize_pipeline_pi.set_storage_buffer(0, m_buffers.counter_hash_table);
    m_pass_instances.reset_voxelize_pipeline_pi.set_storage_buffer(1, m_buffers.active_chunk_keys_list);
    m_pass_instances.reset_voxelize_pipeline_pi.set_storage_buffer(2, m_buffers.triangle_indices_list);
    m_pass_instances.reset_voxelize_pipeline_pi.set_storage_buffer(3, voxel_writes);

    m_pass_instances.reset_voxelize_pipeline_pi.bind(command_buffer);

    m_pass_instances.reset_voxelize_pipeline_pi.push_constants(command_buffer, ResetVoxelizePipelinePushConstants{
        .u_counter_hash_table_size = m_params.counter_hash_table_size,
        .u_reset_voxel_write_list = reset_voxel_write_list ? 1u : 0u
    });

    uint32_t slot_groups = math_utils::div_up_u32(m_params.counter_hash_table_size, 256u);
    command_buffer.dispatch(slot_groups, 1, 1);

    m_buffers.counter_hash_table.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.active_chunk_keys_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.triangle_indices_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    voxel_writes.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void Voxelizator::reset_failure_slots_counter(VulkanCommandBuffer& command_buffer, VulkanBuffer& failure_slots) {
    LOG_METHOD();
    failure_slots.fill(command_buffer, 0, sizeof(uint32_t));
    failure_slots.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void Voxelizator::mark_and_count_active_chunks(
    VulkanCommandBuffer& command_buffer,
    MeshView mesh,
    uint32_t position_attribute_offset,
    uint32_t vertex_stride,
    glm::mat4 transform)
{
    LOG_METHOD();

    m_pass_instances.mark_and_count_active_chunks_pi.set_storage_buffer(0, m_buffers.counter_hash_table);
    m_pass_instances.mark_and_count_active_chunks_pi.set_storage_buffer(1, m_buffers.counter_hash_table_failure_slots);
    m_pass_instances.mark_and_count_active_chunks_pi.set_storage_buffer(2, m_buffers.active_chunk_keys_list);
    m_pass_instances.mark_and_count_active_chunks_pi.set_storage_buffer(3, mesh.vertex_buffer_view().handle());
    m_pass_instances.mark_and_count_active_chunks_pi.set_storage_buffer(4, mesh.index_buffer_view().handle());

    m_pass_instances.mark_and_count_active_chunks_pi.bind(command_buffer);

    uint32_t count_triangles = mesh.index_count() / 3;

    m_pass_instances.mark_and_count_active_chunks_pi.push_constants(command_buffer, MarkAndCountActiveChunksPushConstants{
        .u_voxel_size = glm::vec4(m_params.voxel_size.x, m_params.voxel_size.y, m_params.voxel_size.z, 0),
        .u_chunk_size = glm::uvec4(m_params.chunk_size.x, m_params.chunk_size.y, m_params.chunk_size.z, 0),
        .u_transform = transform,
        .u_count_mesh_triangles = count_triangles,
        .u_counter_hash_table_size = m_params.counter_hash_table_size,
        .u_vertex_stride_bytes = vertex_stride,
        .u_vertex_position_offset_bytes = position_attribute_offset,
        .u_pack_offset = math_utils::OFFSET,
        .u_pack_bits = math_utils::BITS
    });

    uint32_t count_triangle_groups = math_utils::div_up_u32(count_triangles, 256u);
    command_buffer.dispatch(count_triangle_groups, 1, 1);

    m_buffers.counter_hash_table.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.active_chunk_keys_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void Voxelizator::mark_and_count_fail_slots(
    VulkanCommandBuffer& command_buffer,
    const VulkanBuffer& dispatch_args,
    VulkanBuffer& readable_failure_slots,
    VulkanBuffer& writable_failure_slots)
{
    LOG_METHOD();

    m_pass_instances.mark_and_count_fail_slots_pw.set_storage_buffer(0, m_buffers.counter_hash_table);
    m_pass_instances.mark_and_count_fail_slots_pw.set_storage_buffer(1, readable_failure_slots);
    m_pass_instances.mark_and_count_fail_slots_pw.set_storage_buffer(2, writable_failure_slots);
    m_pass_instances.mark_and_count_fail_slots_pw.set_storage_buffer(3, m_buffers.active_chunk_keys_list);

    m_pass_instances.mark_and_count_fail_slots_pw.bind(command_buffer);

    m_pass_instances.mark_and_count_fail_slots_pw.push_constants(command_buffer, MarkAndCountFailSlotsPushConstants{
        .u_counter_hash_table_size = m_params.counter_hash_table_size
    });

    command_buffer.dispatch_indirect(dispatch_args);

    m_buffers.counter_hash_table.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    writable_failure_slots.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.active_chunk_keys_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void Voxelizator::alloc_active_chunk_triangles(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args) {
    LOG_METHOD();

    m_pass_instances.alloc_active_chunk_triangles_pi.set_storage_buffer(0, m_buffers.counter_hash_table);
    m_pass_instances.alloc_active_chunk_triangles_pi.set_storage_buffer(1, m_buffers.active_chunk_keys_list);
    m_pass_instances.alloc_active_chunk_triangles_pi.set_storage_buffer(2, m_buffers.triangle_indices_list);

    m_pass_instances.alloc_active_chunk_triangles_pi.bind(command_buffer);

    m_pass_instances.alloc_active_chunk_triangles_pi.push_constants(command_buffer, AllocActiveChunkTrianglesPushConstants{
        .u_counter_hash_table_size = m_params.counter_hash_table_size
    });

    command_buffer.dispatch_indirect(dispatch_args);

    m_buffers.counter_hash_table.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.active_chunk_keys_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.triangle_indices_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void Voxelizator::fill_triangle_indices(
    VulkanCommandBuffer& command_buffer,
    MeshView mesh,
    uint32_t position_attribute_offset,
    uint32_t vertex_stride,
    glm::mat4 transform) 
{
    LOG_METHOD();

    m_pass_instances.fill_triangle_indices_pi.set_storage_buffer(0, m_buffers.counter_hash_table);
    m_pass_instances.fill_triangle_indices_pi.set_storage_buffer(1, m_buffers.triangle_indices_list);
    m_pass_instances.fill_triangle_indices_pi.set_storage_buffer(2, mesh.vertex_buffer_view().handle());
    m_pass_instances.fill_triangle_indices_pi.set_storage_buffer(3, mesh.index_buffer_view().handle());

    m_pass_instances.fill_triangle_indices_pi.bind(command_buffer);

    uint32_t count_triangles = mesh.index_count() / 3;

    m_pass_instances.fill_triangle_indices_pi.push_constants(command_buffer, FillTriangleIndicesPushConstants{
        .u_voxel_size = glm::vec4(m_params.voxel_size.x, m_params.voxel_size.y, m_params.voxel_size.z, 0),
        .u_chunk_size = glm::uvec4(m_params.chunk_size.x, m_params.chunk_size.y, m_params.chunk_size.z, 0),
        .u_transform = transform,
        .u_count_mesh_triangles = count_triangles,
        .u_counter_hash_table_size = m_params.counter_hash_table_size,
        .u_vertex_stride_bytes = vertex_stride,
        .u_vertex_position_offset_bytes = position_attribute_offset,
        .u_pack_offset = math_utils::OFFSET,
        .u_pack_bits = math_utils::BITS
    });

    uint32_t count_triangle_groups = math_utils::div_up_u32(count_triangles, 256u);
    command_buffer.dispatch(count_triangle_groups, 1, 1);

    m_buffers.counter_hash_table.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.triangle_indices_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void Voxelizator::voxelize_chunks(
    VulkanCommandBuffer& command_buffer,
    const VulkanBuffer& dispatch_args,
    VulkanBuffer& voxel_writes,
    uint32_t voxel_type_flags,
    uint32_t voxel_color,
    uint32_t voxel_set_flags,
    MeshView mesh,
    uint32_t position_attribute_offset,
    uint32_t vertex_stride,
    glm::mat4 transform)
{
    LOG_METHOD();

    m_pass_instances.voxelize_triangles_pi.set_storage_buffer(0, m_buffers.counter_hash_table);
    m_pass_instances.voxelize_triangles_pi.set_storage_buffer(1, m_buffers.active_chunk_keys_list);
    m_pass_instances.voxelize_triangles_pi.set_storage_buffer(2, m_buffers.triangle_indices_list);
    m_pass_instances.voxelize_triangles_pi.set_storage_buffer(3, mesh.vertex_buffer_view().handle());
    m_pass_instances.voxelize_triangles_pi.set_storage_buffer(4, mesh.index_buffer_view().handle());
    m_pass_instances.voxelize_triangles_pi.set_storage_buffer(5, voxel_writes);

    m_pass_instances.voxelize_triangles_pi.bind(command_buffer);

    uint32_t count_triangles = mesh.index_count() / 3;

    m_pass_instances.voxelize_triangles_pi.push_constants(command_buffer, VoxelizeTrianglesPushConstants{
        .u_chunk_dim = glm::uvec4(m_params.chunk_size.x, m_params.chunk_size.y, m_params.chunk_size.z, 0),
        .u_voxel_size = glm::uvec4(m_params.voxel_size.x, m_params.voxel_size.y, m_params.voxel_size.z, 0),
        .u_transform = transform,
        .u_counter_hash_table_size = m_params.counter_hash_table_size,
        .u_count_voxels_in_chunk = static_cast<uint32_t>(m_params.chunk_size.x * m_params.chunk_size.y * m_params.chunk_size.z),
        .u_vertex_stride_bytes = vertex_stride,
        .u_vertex_position_offset_bytes = position_attribute_offset,
        .u_pack_offset = math_utils::OFFSET,
        .u_pack_bits = math_utils::BITS,
        .voxel_type_flags = voxel_type_flags,
        .voxel_color = voxel_color,
        .voxel_set_flags = voxel_set_flags
    });

    command_buffer.dispatch_indirect(dispatch_args);

    m_buffers.counter_hash_table.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.active_chunk_keys_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_buffers.triangle_indices_list.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    voxel_writes.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}
