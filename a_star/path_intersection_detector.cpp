#include "path_intersection_detector.h"

#include "../managers/compute_pass_manager.h"
#include "../math_utils.h"
#include "../voxel_grid_vulkan/voxel_grid.h"
#include "../vulkan_self/push_constants_structures.h"
#include "../vulkan_self/vulkan_command_buffer.h"
#include "../vulkan_self/vulkan_device.h"
#include "../vulkan_self/vulkan_physical_device.h"
#include "../vulkan_self/vulkan_submit_context.h"

PathIntersectionDetector::PathIntersectionDetector(
    const VulkanPhysicalDevice& physical_device,
    VulkanDevice& device,
    VulkanSubmitContext& submit_context,
    ComputePassManager& compute_pass_manager,
    VoxelGrid& voxel_grid,
    uint32_t max_path_points)
    :   m_voxel_grid(&voxel_grid),
        m_physical_device(&physical_device),
        m_device(&device),
        m_max_path_points(max_path_points),
        m_pass_instances(create_pass_instances(device, compute_pass_manager)),
        m_buffers(create_buffers(physical_device, device, submit_context)) {}

void PathIntersectionDetector::realloc_buffers(
    VulkanSubmitContext& submit_context,
    uint32_t max_path_points)
{
    LOG_METHOD();

    m_max_path_points = max_path_points;

    logger().check(m_physical_device != nullptr, "Physical device pointer specify to null");
    logger().check(m_device != nullptr, "Device pointer specify to null");

    m_buffers = create_buffers(*m_physical_device, *m_device, submit_context);
}

bool PathIntersectionDetector::is_path_passable(
    VulkanSubmitContext& submit_context,
    const std::vector<glm::vec3>& path_points)
{
    LOG_METHOD();

    if (path_points.size() <= 1) {
        return true;
    }

    if (path_points.size() > m_max_path_points) {
        realloc_buffers(
            submit_context,
            static_cast<uint32_t>(path_points.size() * 2)
        );
    }

    std::vector<glm::vec4> gpu_path_points;
    gpu_path_points.reserve(path_points.size());
    for (const glm::vec3& point : path_points) {
        gpu_path_points.emplace_back(point, 0.0f);
    }

    m_buffers.path_points.upload(gpu_path_points);

    {
        auto scope = submit_context.submit_and_wait_scope();
        check_path_passability(
            scope.command_buffer(),
            static_cast<uint32_t>(path_points.size())
        );
    }

    return m_buffers.path_passability.read_scalar<uint32_t>() != 0u;
}

bool PathIntersectionDetector::has_intersection(
    VulkanSubmitContext& submit_context,
    const std::vector<glm::vec3>& path_points)
{
    LOG_METHOD();

    return !is_path_passable(submit_context, path_points);
}

PathIntersectionDetector::DetectorPassInstances PathIntersectionDetector::create_pass_instances(
    VulkanDevice& device,
    ComputePassManager& compute_pass_manager) const
{
    LOG_METHOD();

    DescriptorPool& dp = compute_pass_manager.descriptor_pool();

    return DetectorPassInstances{
        .check_path_passability_pi = PassInstance(compute_pass_manager.check_path_passability_cp, dp)
    };
}

PathIntersectionDetector::DetectorBuffers PathIntersectionDetector::create_buffers(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VulkanSubmitContext& submit_context) const
{
    LOG_METHOD();

    logger().check(m_max_path_points > 0u, "Max path points must be greater than 0");

    VkDeviceSize path_points_size = sizeof(glm::vec4) * m_max_path_points;
    VkDeviceSize path_passability_size = sizeof(uint32_t);

    return DetectorBuffers{
        .path_points = VulkanBuffer(
            physical_device,
            device,
            path_points_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        ),
        .path_passability = VulkanBuffer(
            physical_device,
            device,
            path_passability_size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        )
    };
}

void PathIntersectionDetector::check_path_passability(
    VulkanCommandBuffer& command_buffer,
    uint32_t count_path_points)
{
    LOG_METHOD();

    logger().check(m_voxel_grid != nullptr, "Voxel grid pointer specify to null");
    logger().check(count_path_points > 1u, "Count path points must be greater than 1");

    m_pass_instances.check_path_passability_pi.set_storage_buffer(0, m_buffers.path_points);
    m_pass_instances.check_path_passability_pi.set_storage_buffer(1, m_buffers.path_passability);
    m_pass_instances.check_path_passability_pi.set_storage_buffer(2, m_voxel_grid->buffers().chunk_hash_table);
    m_pass_instances.check_path_passability_pi.set_storage_buffer(3, m_voxel_grid->buffers().voxels);

    m_buffers.path_passability.fill(
        command_buffer,
        1u,
        sizeof(uint32_t)
    );
    m_buffers.path_passability.transfer_write_to_compute_read_write_barrier(
        command_buffer,
        0,
        sizeof(uint32_t)
    );

    m_pass_instances.check_path_passability_pi.bind(command_buffer);

    const auto& voxel_grid_params = m_voxel_grid->params();

    m_pass_instances.check_path_passability_pi.push_constants(command_buffer, CheckPathPassabilityPushConstants{
        .u_chunk_dim = glm::ivec4(voxel_grid_params.chunk_size, 0),
        .u_voxel_size = glm::vec4(voxel_grid_params.voxel_size, 0.0f),
        .u_count_path_points = count_path_points,
        .u_max_step_up = 0u,
        .u_max_drop = 0u,
        .u_chunk_hash_table_size = voxel_grid_params.chunk_hash_table_size,
        .u_voxels_per_chunk = voxel_grid_params.chunk_size.x * voxel_grid_params.chunk_size.y * voxel_grid_params.chunk_size.z,
        .u_pack_offset = static_cast<uint32_t>(math_utils::OFFSET),
        .u_pack_bits = math_utils::BITS,
        .u_allow_flying_over_precipices = 0u,
        .u_allow_diagonal_moves = 1u
    });

    uint32_t segment_count = count_path_points - 1u;
    uint32_t groups = math_utils::div_up_u32(segment_count, 256u);
    command_buffer.dispatch(groups, 1, 1);

    m_buffers.path_passability.memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_HOST_BIT,
        VK_ACCESS_HOST_READ_BIT,
        0,
        sizeof(uint32_t)
    );
}
