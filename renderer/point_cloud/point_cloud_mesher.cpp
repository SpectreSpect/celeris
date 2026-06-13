#include "point_cloud_mesher.h"

#include "../../vulkan_self/vulkan_device.h"
#include "../../managers/compute_pass_manager.h"
#include "../../vulkan_self/vulkan_queue.h"

PointCloudMesher::PointCloudMesher(
    const VulkanPhysicalDevice& physical_device,
    VulkanDevice& device,
    VulkanQueue& queue,
    ComputePassManager& compute_pass_manager,
    uint32_t count_points_in_lidar_ring)
    :   m_queue(&queue),
        m_command_pool(device, *m_queue),
        m_command_buffer(device, m_command_pool),
        m_fence(device),
        m_generate_mesh_pw(device, compute_pass_manager.generate_mesh_cp),
        m_valid_triangle_count_buffer(VulkanBuffer::create_host_visible_transfer_dst_storage_buffer(physical_device, device, sizeof(uint32_t))),
        m_count_points_in_lidar_ring(count_points_in_lidar_ring) {}

void PointCloudMesher::submit_compute_commands() {
    LOG_METHOD();

    logger.check(m_queue != nullptr, "Queue pointer specify to null");

    m_fence.reset();
    m_queue->submit(m_command_buffer, &m_fence);
    m_fence.wait();
    m_command_buffer.reset();
}
