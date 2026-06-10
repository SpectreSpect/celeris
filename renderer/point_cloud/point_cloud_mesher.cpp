#include "point_cloud_mesher.h"

#include <string>

#include "../../vulkan_self/vulkan_device.h"
#include "../../managers/compute_pass_manager.h"
#include "../../vulkan_self/vulkan_queue.h"
#include "../../vulkan_self/push_constants_structures.h"
#include "point_cloud.h"
#include "../../math_utils.h"

PointCloudMesher::PointCloudMesher(
    const VulkanDevice& device,
    VulkanQueue& queue,
    ComputePassManager& compute_pass_manager,
    uint32_t count_points_in_lidar_ring)
    :   m_queue(&queue),
        m_command_pool(device, *m_queue),
        m_command_buffer(device, m_command_pool),
        m_fence(device),
        m_generate_mesh_pi(compute_pass_manager.generate_mesh_cp, compute_pass_manager.descriptor_pool()),
        m_count_points_in_lidar_ring(count_points_in_lidar_ring) {}

void PointCloudMesher::convert_to_mesh(
    VulkanCommandBuffer& command_buffer,
    const PointCloud& point_cloud, 
    VulkanBuffer& vertex_buffer, 
    VulkanBuffer& index_buffer,
    uint32_t point_stride_bytes,
    uint32_t point_position_offset_bytes,
    uint32_t vertex_stride_bytes,
    uint32_t vertex_position_offset_bytes, 
    uint32_t vertex_normal_offset_bytes,
    uint32_t vertex_color_offset_bytes)
{
    LOG_METHOD();

    m_generate_mesh_pi.set_storage_buffer(0, point_cloud.instance_buffer());
    m_generate_mesh_pi.set_storage_buffer(1, vertex_buffer);
    m_generate_mesh_pi.set_storage_buffer(2, index_buffer);

    m_generate_mesh_pi.bind(command_buffer);

    uint32_t count_triangles_in_lidar_ring = m_count_points_in_lidar_ring * 2 - 2;

    m_generate_mesh_pi.push_constants(command_buffer, GenerateMeshPushConstants{
        .count_triangles_in_lidar_ring = count_triangles_in_lidar_ring,

        .point_stride_bytes = point_stride_bytes,
        .point_position_offset_bytes = point_position_offset_bytes,
        
        .vertex_stride_bytes = vertex_stride_bytes,
        .vertex_position_offset_bytes = vertex_position_offset_bytes,
        .vertex_normal_offset_bytes = vertex_normal_offset_bytes,
        .vertex_color_offset_bytes = vertex_color_offset_bytes
    });

    uint32_t count_rings = point_cloud.point_count() / m_count_points_in_lidar_ring;
    uint32_t count_rings_reminder = point_cloud.point_count() % m_count_points_in_lidar_ring;

    logger.check(count_rings_reminder == 0)
        << "The point cloud contains a non-integer number of rings ("
        << clr(std::to_string(count_rings), LoggerPalette::blue) << "integers and "
        << clr(std::to_string(count_rings_reminder), LoggerPalette::orange) << " remainder)."
        << "Maybe the number of points in the lidar ring ("
        << clr(std::to_string(m_count_points_in_lidar_ring), LoggerPalette::yellow) << ") is incorrect?";

    uint32_t count_triangles_in_ring_group = math_utils::div_up_u32(count_triangles_in_lidar_ring, 256u);
    
    command_buffer.dispatch(count_triangles_in_ring_group, count_rings - 1, 1);

    vertex_buffer.memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
    );
    index_buffer.memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_INDEX_READ_BIT
    );
}

void PointCloudMesher::convert_to_mesh(
    const PointCloud& point_cloud,
    VulkanBuffer& vertex_buffer,
    VulkanBuffer& index_buffer,
    uint32_t point_stride_bytes,
    uint32_t point_position_offset_bytes,
    uint32_t vertex_stride_bytes,
    uint32_t vertex_position_offset_bytes, 
    uint32_t vertex_normal_offset_bytes,
    uint32_t vertex_color_offset_bytes)
{
    LOG_METHOD();

    {
        auto scope = m_command_buffer.begin_scope();
        convert_to_mesh(
            m_command_buffer,
            point_cloud,
            vertex_buffer,
            index_buffer,
            point_stride_bytes,
            point_position_offset_bytes,
            vertex_stride_bytes,
            vertex_position_offset_bytes, 
            vertex_normal_offset_bytes,
            vertex_color_offset_bytes
        );
    }
    submit_compute_commands();
}

void PointCloudMesher::submit_compute_commands() {
    LOG_METHOD();

    logger.check(m_queue != nullptr, "Queue pointer specify to null");

    m_fence.reset();
    m_queue->submit(m_command_buffer, &m_fence);
    m_fence.wait();
    m_command_buffer.reset();
}