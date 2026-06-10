#pragma once

#include <optional>

#include "../../vulkan_self/logger/logger_header.h"
#include "../../vulkan_self/pass/instance/pass_instance.h"
#include "../../vulkan_self/vulkan_command_buffer.h"
#include "../../vulkan_self/vulkan_command_pool.h"
#include "../../vulkan_self/vulkan_fence.h"

class VulkanDevice;
class ComputePassManager;
class VulkanQueue;
class PointCloud;
class VulkanBuffer;

inline constexpr uint32_t DONT_SET_COLOR = 0xFFFFFFFFu;

class PointCloudMesher {
public:
    _XCLASS_NAME(PointCloudMesher);

    explicit PointCloudMesher(
        const VulkanDevice& device,
        VulkanQueue& queue,
        ComputePassManager& compute_pass_manager,
        uint32_t count_points_in_lidar_ring
    );
    ~PointCloudMesher() noexcept = default;

    PointCloudMesher(const PointCloudMesher&) = delete;
    PointCloudMesher& operator=(const PointCloudMesher&) = delete;

    PointCloudMesher(PointCloudMesher&&) noexcept = default;
    PointCloudMesher& operator=(PointCloudMesher&&) noexcept = default;

    void convert_to_mesh(
        VulkanCommandBuffer& command_buffer,
        const PointCloud& point_cloud,
        VulkanBuffer& vertex_buffer,
        VulkanBuffer& index_buffer,
        uint32_t point_stride_bytes,
        uint32_t point_position_offset_bytes,
        uint32_t vertex_stride_bytes,
        uint32_t vertex_position_offset_bytes, 
        uint32_t vertex_normal_offset_bytes,
        uint32_t vertex_color_offset_bytes = DONT_SET_COLOR
    );

    void convert_to_mesh(
        const PointCloud& point_cloud,
        VulkanBuffer& vertex_buffer,
        VulkanBuffer& index_buffer,
        uint32_t point_stride_bytes,
        uint32_t point_position_offset_bytes,
        uint32_t vertex_stride_bytes,
        uint32_t vertex_position_offset_bytes, 
        uint32_t vertex_normal_offset_bytes,
        uint32_t vertex_color_offset_bytes = DONT_SET_COLOR
    );

    void submit_compute_commands();

private:
    VulkanQueue* m_queue = nullptr;

    VulkanCommandPool m_command_pool;
    VulkanCommandBuffer m_command_buffer;
    VulkanFence m_fence;

    PassInstance m_generate_mesh_pi;

    uint32_t m_count_points_in_lidar_ring = 0;
};
