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
        uint32_t vertex_color_offset_bytes
    );

    template<class T>
    void convert_to_mesh(
        const PointCloud& point_cloud,
        VulkanBuffer& vertex_buffer,
        VulkanBuffer& index_buffer,
        uint32_t point_stride_bytes,
        uint32_t point_position_offset_bytes)
    {
        LOG_METHOD();

        static_assert(
            std::is_standard_layout_v<T>,
            "Vertex type must be a standard-layout type!"
        );
        static_assert(
            requires(T v) { v.position; },
            "Vertex type must contain a 'position' field!"
        );
        static_assert(
            requires(T v) { v.normal; },
            "Vertex type must contain a 'normal' field!"
        );

        constexpr uint32_t vertex_stride_bytes = sizeof(T);
        constexpr uint32_t vertex_position_offset_bytes = offsetof(T, position);
        constexpr uint32_t vertex_normal_offset_bytes = offsetof(T, normal);
        constexpr uint32_t vertex_color_offset_bytes = DONT_SET_COLOR;

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

    void submit_compute_commands();

private:
    VulkanQueue* m_queue = nullptr;

    VulkanCommandPool m_command_pool;
    VulkanCommandBuffer m_command_buffer;
    VulkanFence m_fence;

    PassInstance m_generate_mesh_pi;

    uint32_t m_count_points_in_lidar_ring = 0;
};
