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
        m_window_size(window_size),
        m_max_astar_points(max_astar_points),
        m_pass_instances(create_pass_instances(device, compute_pass_manager)),
        m_buffers(create_buffers(physical_device, device, submit_context)) {}

void UnimpendedPathFinder::realloc_buffers(
    const VulkanPhysicalDevice& physical_device,
    VulkanDevice& device,
    VulkanSubmitContext& submit_context,
    uint32_t window_size,
    uint32_t max_astar_points)
{
    LOG_METHOD();

    m_window_size = window_size;
    m_max_astar_points = max_astar_points;

    m_buffers = create_buffers(physical_device, device, submit_context);
}

std::vector<glm::vec3> UnimpendedPathFinder::find_unimpended_path(
    const VulkanPhysicalDevice& physical_device,
    VulkanDevice& device,
    VulkanSubmitContext& submit_context,
    std::vector<glm::vec4> astar_path, 
    uint32_t max_step_height,
    uint32_t start_id)
{
    LOG_METHOD();

    if (astar_path.size() > m_max_astar_points) {
        realloc_buffers(
            physical_device,
            device,
            submit_context,
            m_window_size,
            astar_path.size() * 2 // *2 чтобы уменьшить вероятность перевыделения
        );
    }

    m_buffers.astar_path.upload(astar_path);

    {
        auto scope = submit_context.submit_and_wait_scope();
        fill_adjacency_matrix(
            scope.command_buffer(),
            static_cast<uint32_t>(astar_path.size()),
            max_step_height,
            start_id
        );
    }


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

    // -1 потому что связь в себя не считается
    VkDeviceSize windowed_adjacency_matrix_size = sizeof(uint32_t) * m_max_astar_points * (m_window_size - 1);
    VkDeviceSize astar_path_size = sizeof(uint32_t) * 4 + sizeof(glm::ivec4) * m_max_astar_points;

    VulkanBuffer astar_path = VulkanBuffer(
        physical_device,
        device,
        astar_path_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    {
        auto scope = submit_context.submit_and_wait_scope();

        astar_path.fill(scope.command_buffer(), 0u, sizeof(uint32_t));
    }
    
    return FinderBuffers{
        .windowed_adjacency_matrix = VulkanBuffer(
            physical_device,
            device,
            windowed_adjacency_matrix_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        ),
        .astar_path = std::move(astar_path),
    };
}

void UnimpendedPathFinder::fill_adjacency_matrix(
    VulkanCommandBuffer& command_buffer,
    uint32_t count_astar_points,
    uint32_t max_step_height,
    uint32_t start_id)
{
    LOG_METHOD();

    logger.check(m_voxel_grid != nullptr, "Voxel grid pointer specify to null");

    m_pass_instances.find_unimpended_paths_pi.set_storage_buffer(0, m_buffers.astar_path);
    m_pass_instances.find_unimpended_paths_pi.set_storage_buffer(1, m_buffers.windowed_adjacency_matrix);
    m_pass_instances.find_unimpended_paths_pi.set_storage_buffer(2, m_voxel_grid->buffers().chunk_hash_table);
    m_pass_instances.find_unimpended_paths_pi.set_storage_buffer(3, m_voxel_grid->buffers().voxels);

    m_pass_instances.find_unimpended_paths_pi.bind(command_buffer);

    m_pass_instances.find_unimpended_paths_pi.push_constants(command_buffer, FindUnimpendedPathsPushConstants{
        .u_window_size = m_window_size, // Количество точек в окне, но кроме текущей точки
        .u_start_id = start_id,
        .u_max_step_height = max_step_height,
        .u_count_astar_points = count_astar_points
    });

    uint32_t from_local_id_groups = math_utils::div_up_u32(count_astar_points - start_id, 256u);
    command_buffer.dispatch(from_local_id_groups, m_window_size - 1, 1);

    m_buffers.windowed_adjacency_matrix.memory_barrier_compute_write_to_compute_write_read(command_buffer);
    m_voxel_grid->buffers().chunk_hash_table.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

std::vector<glm::vec3> UnimpendedPathFinder::build_path_from_adjacency_matrix(
    std::vector<glm::vec4> astar_path
) {
    LOG_METHOD();

    std::vector<uint32_t > astar_indices = m_buffers.windowed_adjacency_matrix.read_vector<uint32_t>(
        astar_path.size() * (m_window_size - 1)
    );

    std::vector<glm::vec3> unimpended_path;

    uint32_t current_id = 0u;
    while (current_id != astar_path.size() - 1) {
        unimpended_path.push_back();
    }
}
