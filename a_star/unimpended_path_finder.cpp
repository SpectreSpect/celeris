#include "unimpended_path_finder.h"

#include "../vulkan_self/vulkan_command_buffer.h"
#include "../vulkan_self/vulkan_submit_context.h"
#include "../voxel_grid_vulkan/voxel_grid.h"
#include "../vulkan_self/vulkan_physical_device.h"
#include "../vulkan_self/vulkan_device.h"
#include "../managers/compute_pass_manager.h"
#include "../vulkan_self/push_constants_structures.h"
#include "../math_utils.h"

UnimpendedPathFinder::UnimpendedPathFinder(
    const VulkanPhysicalDevice& physical_device,
    VulkanDevice& device,
    VulkanSubmitContext& submit_context,
    ComputePassManager& compute_pass_manager,
    VoxelGrid& voxel_grid,
    uint32_t window_size,
    uint32_t max_astar_points)
    :   m_voxel_grid(&voxel_grid),
        m_physical_device(&physical_device),
        m_device(&device),
        m_window_size(window_size),
        m_max_astar_points(max_astar_points),
        m_pass_instances(create_pass_instances(device, compute_pass_manager)),
        m_buffers(create_buffers(physical_device, device, submit_context)) {}

void UnimpendedPathFinder::realloc_buffers(
    VulkanSubmitContext& submit_context,
    uint32_t window_size,
    uint32_t max_astar_points)
{
    LOG_METHOD();

    m_window_size = window_size;
    m_max_astar_points = max_astar_points;

    logger().check(m_physical_device != nullptr, "Physical device pointer specify to null");
    logger().check(m_device != nullptr, "Device pointer specify to null");

    m_buffers = create_buffers(*m_physical_device, *m_device, submit_context);
}

std::vector<glm::ivec3> UnimpendedPathFinder::find_unimpended_path(
    VulkanSubmitContext& submit_context,
    const std::vector<glm::ivec4>& astar_path, 
    uint32_t max_step_up,
    uint32_t max_drop,
    bool allow_flying_over_precipices,
    uint32_t start_id)
{
    LOG_METHOD();

    if (astar_path.empty()) {
        return {};
    }

    if (astar_path.size() > m_max_astar_points) {
        realloc_buffers(
            submit_context,
            m_window_size,
            astar_path.size() * 2 // *2 чтобы уменьшить вероятность перевыделения
        );
    }

    m_buffers.astar_path.upload(astar_path);

    {
        auto scope = submit_context.submit_and_wait_scope();
        fill_max_unimpended_path_indices(
            scope.command_buffer(),
            static_cast<uint32_t>(astar_path.size()),
            max_step_up,
            max_drop,
            allow_flying_over_precipices,
            start_id
        );
    }

    return build_path_from_max_unimpended_path_indices(astar_path);
}

UnimpendedPathFinder::FinderPassInstances UnimpendedPathFinder::create_pass_instances(
    VulkanDevice& device,
    ComputePassManager& compute_pass_manager) const 
{
    LOG_METHOD();

    DescriptorPool& dp = compute_pass_manager.descriptor_pool();

    return FinderPassInstances{
        .find_unimpended_paths_pi = PassInstance(compute_pass_manager.find_unimpended_paths_cp, dp)
    };
}

UnimpendedPathFinder::FinderBuffers UnimpendedPathFinder::create_buffers(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VulkanSubmitContext& submit_context) const
{
    LOG_METHOD();

    VkDeviceSize max_unimpended_path_indices_size = sizeof(uint32_t) * m_max_astar_points;
    VkDeviceSize astar_path_size = sizeof(glm::ivec4) * m_max_astar_points;
    
    return FinderBuffers{
        .max_unimpended_path_indices = VulkanBuffer(
            physical_device,
            device,
            max_unimpended_path_indices_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        ),
        .astar_path = VulkanBuffer(
            physical_device,
            device,
            astar_path_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        )
    };
}

void UnimpendedPathFinder::fill_max_unimpended_path_indices(
    VulkanCommandBuffer& command_buffer,
    uint32_t count_astar_points,
    uint32_t max_step_up,
    uint32_t max_drop,
    bool allow_flying_over_precipices,
    uint32_t start_id)
{
    LOG_METHOD();

    logger().check(m_voxel_grid != nullptr, "Voxel grid pointer specify to null");

    m_pass_instances.find_unimpended_paths_pi.set_storage_buffer(0, m_buffers.astar_path);
    m_pass_instances.find_unimpended_paths_pi.set_storage_buffer(1, m_buffers.max_unimpended_path_indices);
    m_pass_instances.find_unimpended_paths_pi.set_storage_buffer(2, m_voxel_grid->buffers().chunk_hash_table);
    m_pass_instances.find_unimpended_paths_pi.set_storage_buffer(3, m_voxel_grid->buffers().voxels);

    m_buffers.max_unimpended_path_indices.fill(
        command_buffer,
        0u,
        sizeof(uint32_t) * count_astar_points
    );
    m_buffers.max_unimpended_path_indices.transfer_write_to_compute_read_write_barrier(command_buffer);

    m_pass_instances.find_unimpended_paths_pi.bind(command_buffer);

    const auto& voxel_grid_params = m_voxel_grid->params();

    m_pass_instances.find_unimpended_paths_pi.push_constants(command_buffer, FindUnimpendedPathsPushConstants{
        .u_chunk_dim = glm::ivec4(voxel_grid_params.chunk_size, 0),
        .u_exclusive_window_size = m_window_size - 1u,
        .u_start_id = start_id,
        .u_max_step_up = max_step_up,
        .u_max_drop = max_drop,
        .u_count_astar_points = count_astar_points,
        .u_chunk_hash_table_size = voxel_grid_params.chunk_hash_table_size,
        .u_voxels_per_chunk = voxel_grid_params.chunk_size.x * voxel_grid_params.chunk_size.y * voxel_grid_params.chunk_size.z,
        .u_pack_offset = static_cast<uint32_t>(math_utils::OFFSET),
        .u_pack_bits = math_utils::BITS,
        .u_allow_flying_over_precipices = allow_flying_over_precipices ? 1u : 0u
    });

    uint32_t from_local_id_groups = math_utils::div_up_u32(count_astar_points - start_id, 256u);
    command_buffer.dispatch(from_local_id_groups, m_window_size - 1, 1);

    m_buffers.max_unimpended_path_indices.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_voxel_grid->buffers().chunk_hash_table.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

std::vector<glm::ivec3> UnimpendedPathFinder::build_path_from_max_unimpended_path_indices(
    const std::vector<glm::ivec4>& astar_path)
{
    LOG_METHOD();

    std::vector<uint32_t> astar_indices = m_buffers.max_unimpended_path_indices.read_vector<uint32_t>(astar_path.size());

    std::vector<glm::ivec3> unimpended_path;
    if (astar_path.empty()) {
        return unimpended_path;
    }

    const uint32_t last_id = static_cast<uint32_t>(astar_path.size() - 1);
    uint32_t current_id = 0u;
    while (current_id <= last_id) {
        unimpended_path.push_back(glm::ivec3(astar_path[current_id]));
        if (current_id == last_id) {
            break;
        }

        uint32_t next_id = astar_indices[current_id];
        if (next_id <= current_id || next_id > last_id) {
            break;
        }

        current_id = next_id;
    }

    return unimpended_path;
}
