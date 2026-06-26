#include "point_cloud_preprocessor.h"

#include "../../vulkan_self/push_constants_structures.h"
#include "../../managers/compute_pass_manager.h"
#include "../../vulkan_self/vulkan_buffer.h"
#include "../../math_utils.h"

PointCloudPreprocessor::PointCloudPreprocessor(VulkanDevice& device, VulkanQueue& queue, ComputePassManager& compute_pass_manager) 
    :   m_command_pool(device, queue),
        m_command_buffer(device, m_command_pool),
        m_fence(device),
        m_queue(&queue),
        m_pass_instances(create_pass_instances(device, compute_pass_manager)){
    
}

PointCloudPreprocessor::PointCloudPreprocessorPassInstances PointCloudPreprocessor::create_pass_instances(VulkanDevice& device, 
                                                         ComputePassManager& compute_pass_manager) const {
    LOG_METHOD();
    
    DescriptorPool& dp = compute_pass_manager.descriptor_pool();

    return PointCloudPreprocessorPassInstances {
        .normals_from_webots_lidar_point_cloud_pi = PassInstance(
                compute_pass_manager.normals_from_webots_lidar_point_cloud_cp, dp),
        .remove_near_origin_lidar_points_pi = PassInstance(
                compute_pass_manager.remove_near_origin_lidar_points_cp, dp)
    };
}

void PointCloudPreprocessor::get_normals_from_webots_lidar_point_cloud(VulkanBuffer& source_point_instance_buffer, 
                                                                       VulkanBuffer& output_normal_buffer,
                                                                       uint32_t point_count,
                                                                       uint32_t ring_count) {
    {
        auto scope = m_command_buffer.begin_scope();
        get_normals_from_webots_lidar_point_cloud(m_command_buffer, 
                                                  source_point_instance_buffer, 
                                                  output_normal_buffer, 
                                                  point_count,
                                                  ring_count);
    }
    submit_compute_commands();
}

void PointCloudPreprocessor::remove_points_near_origin(VulkanBuffer& point_instance_buffer,
                                                       uint32_t point_count,
                                                       float min_distance) {
    {
        auto scope = m_command_buffer.begin_scope();
        remove_points_near_origin(m_command_buffer,
                                  point_instance_buffer,
                                  point_count,
                                  min_distance);
    }
    submit_compute_commands();
}

void PointCloudPreprocessor::remove_points_near_origin(VulkanCommandBuffer& command_buffer,
                                                       VulkanBuffer& point_instance_buffer,
                                                       uint32_t point_count,
                                                       float min_distance) {
    LOG_METHOD();

    logger.check(point_count > 0, "Point cloud had 0 points");

    PassInstance& pi = m_pass_instances.remove_near_origin_lidar_points_pi;

    pi.set_storage_buffer(0, point_instance_buffer);
    pi.push_constants(command_buffer, RemoveNearOriginLidarPointsPushConstants{
            .point_count = point_count,
            .min_distance = min_distance
        });

    pi.bind(command_buffer);

    uint32_t x_groups = math_utils::div_up_u32(point_count, 256);
    command_buffer.dispatch(x_groups, 1, 1);

    point_instance_buffer.memory_barrier(
        command_buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT);
}

void PointCloudPreprocessor::get_normals_from_webots_lidar_point_cloud(VulkanCommandBuffer& command_buffer, 
                                                                  VulkanBuffer& source_point_instance_buffer, 
                                                                  VulkanBuffer& output_normal_buffer,
                                                                  uint32_t point_count,
                                                                  uint32_t ring_count) {
    LOG_METHOD();

    logger.check(point_count > 0, "Point cloud had 0 points");

    PassInstance& pi = m_pass_instances.normals_from_webots_lidar_point_cloud_pi;

    pi.set_storage_buffer(0, source_point_instance_buffer);
    pi.set_storage_buffer(1, output_normal_buffer);

    pi.push_constants(m_command_buffer, NormalsFromWebotsLidarPointCloudPushConstants{
            .point_count = point_count,
            .ring_count = ring_count,
            .ring_width = point_count / ring_count
        });
    
    pi.bind(command_buffer);

    uint32_t x_groups = math_utils::div_up_u32(point_count, 256);
    
    command_buffer.dispatch(x_groups, 1, 1);

    output_normal_buffer.memory_barrier_compute_write_to_compute_write_read(command_buffer);
}

void PointCloudPreprocessor::submit_compute_commands() {
    LOG_METHOD();

    logger.check(m_queue != nullptr, "PointCloudPreprocessor queue was not initialized");

    m_fence.reset();
    m_queue->submit(m_command_buffer, &m_fence);
    m_fence.wait();
    m_command_buffer.reset();
}