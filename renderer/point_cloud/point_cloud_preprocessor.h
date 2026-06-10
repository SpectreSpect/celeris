#pragma once

#include "../../vulkan_self/vulkan_command_pool.h"
#include "../../vulkan_self/vulkan_command_buffer.h"
#include "../../vulkan_self/vulkan_fence.h"
#include "../../vulkan_self/pass/instance/pass_instance.h"

#include "../../vulkan_self/logger/logger_header.h"

class ComputePassManager;
class PointCloudPreprocessor {
public:
    _XCLASS_NAME(PointCloudPreprocessor);

    struct PointCloudPreprocessorPassInstances {
        PassInstance normals_from_webots_lidar_point_cloud_pi;
        PassInstance remove_near_origin_lidar_points_pi;
    };

    PointCloudPreprocessor(VulkanDevice& device, VulkanQueue& queue, ComputePassManager& compute_pass_manager);
    
    void get_normals_from_webots_lidar_point_cloud(VulkanBuffer& source_point_instance_buffer, 
                                                   VulkanBuffer& output_normal_buffer,
                                                   uint32_t point_count,
                                                   uint32_t ring_count);

    void get_normals_from_webots_lidar_point_cloud(VulkanCommandBuffer& command_buffer,
                                                   VulkanBuffer& source_point_instance_buffer, 
                                                   VulkanBuffer& output_normal_buffer,
                                                   uint32_t point_count,
                                                   uint32_t ring_count);

    void remove_points_near_origin(VulkanBuffer& point_instance_buffer,
                                   uint32_t point_count,
                                   float min_distance = 2.7f);

    void remove_points_near_origin(VulkanCommandBuffer& command_buffer,
                                   VulkanBuffer& point_instance_buffer,
                                   uint32_t point_count,
                                   float min_distance = 2.7f);

private:
    VulkanCommandPool m_command_pool;
    VulkanCommandBuffer m_command_buffer;
    VulkanFence m_fence;
    VulkanQueue* m_queue = nullptr;

    PointCloudPreprocessorPassInstances m_pass_instances;

    PointCloudPreprocessorPassInstances create_pass_instances(VulkanDevice& device, 
                                                         ComputePassManager& compute_pass_manager) const;
    void submit_compute_commands();
};