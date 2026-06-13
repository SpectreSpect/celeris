#pragma once

#include <optional>
#include <cstdint>
#include <string>
#include <vulkan/vulkan.h>
#include <concepts>
#include <type_traits>

#include "../../vulkan_self/logger/logger_header.h"
#include "../../vulkan_self/pass/instance/pass_writer.h"
#include "../../vulkan_self/vulkan_command_buffer.h"
#include "../../vulkan_self/vulkan_command_pool.h"
#include "../../vulkan_self/vulkan_fence.h"
#include "../../vulkan_self/vulkan_buffer.h"
#include "../../vulkan_self/push_constants_structures.h"
#include "point_cloud.h"
#include "../../math_utils.h"

class VulkanDevice;
class VulkanPhysicalDevice;
class ComputePassManager;
class VulkanQueue;

inline constexpr uint32_t DONT_SET_COLOR = 0xFFFFFFFFu;

class PointCloudMesher {
public:
    _XCLASS_NAME(PointCloudMesher);

    explicit PointCloudMesher(
        const VulkanPhysicalDevice& physical_device,
        VulkanDevice& device,
        VulkanQueue& queue,
        ComputePassManager& compute_pass_manager,
        uint32_t count_points_in_lidar_ring
    );
    ~PointCloudMesher() noexcept = default;

    PointCloudMesher(const PointCloudMesher&) = delete;
    PointCloudMesher& operator=(const PointCloudMesher&) = delete;

    PointCloudMesher(PointCloudMesher&&) noexcept = default;
    PointCloudMesher& operator=(PointCloudMesher&&) noexcept = default;

    template<class Vertex, class PointInstance>
    void convert_to_mesh(
        VulkanCommandBuffer& command_buffer,
        const PointCloud& point_cloud, 
        VulkanBuffer& vertex_buffer, 
        VulkanBuffer& index_buffer)
    {
        LOG_METHOD();

        static_assert(
            std::is_standard_layout_v<Vertex>,
            "Vertex must be a standard-layout type"
        );
        static_assert(
            std::is_trivially_copyable_v<Vertex>,
            "Vertex must be a trivially copyable type"
        );

        static_assert(
            std::is_standard_layout_v<PointInstance>,
            "Vertex must be a standard-layout type"
        );
        static_assert(
            std::is_trivially_copyable_v<PointInstance>,
            "Vertex must be a trivially copyable type"
        );

        static_assert(
            requires(const Vertex& obj){{obj.position} -> std::same_as<const glm::vec4&>;}, 
            "Vertex must have glm::vec4 position"
        );
        static_assert(
            requires(const Vertex& obj){{obj.normal} -> std::same_as<const glm::vec4&>;}, 
            "Vertex must have glm::vec4 normal"
        );
        static_assert(
            requires(const PointInstance& obj){{obj.position} -> std::same_as<const glm::vec4&>;}, 
            "PointInstance must have glm::vec4 position"
        );

        constexpr uint32_t point_stride_bytes = sizeof(PointInstance);
        constexpr uint32_t point_position_offset_bytes = offsetof(PointInstance, position);

        constexpr uint32_t vertex_stride_bytes = sizeof(Vertex);
        constexpr uint32_t vertex_position_offset_bytes = offsetof(Vertex, position);
        constexpr uint32_t vertex_normal_offset_bytes = offsetof(Vertex, normal);

        uint32_t vertex_color_offset_bytes = DONT_SET_COLOR;

        if constexpr (requires(const Vertex& obj){obj.color;}) {
            static_assert(
                requires(const Vertex& obj){{obj.color} -> std::same_as<const glm::vec4&>;}, 
                "Vertex has a color field, but its type does not match glm::vec4"
            );

            vertex_color_offset_bytes = offsetof(Vertex, color);
        }

        m_valid_triangle_count_buffer.fill(command_buffer, 0u, sizeof(uint32_t));
        m_valid_triangle_count_buffer.transfer_write_to_compute_read_write_barrier(command_buffer, 0u, sizeof(uint32_t));

        m_generate_mesh_pw.set_storage_buffer(0, point_cloud.instance_buffer());
        m_generate_mesh_pw.set_storage_buffer(1, vertex_buffer);
        m_generate_mesh_pw.set_storage_buffer(2, index_buffer);
        m_generate_mesh_pw.set_storage_buffer(3, m_valid_triangle_count_buffer);

        m_generate_mesh_pw.bind(command_buffer);

        uint32_t count_triangles_in_lidar_ring = m_count_points_in_lidar_ring * 2 - 2;

        m_generate_mesh_pw.push_constants(command_buffer, GenerateMeshPushConstants{
            .count_triangles_in_lidar_ring = count_triangles_in_lidar_ring,
            .count_points_in_lidar_ring = m_count_points_in_lidar_ring,

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
        m_valid_triangle_count_buffer.memory_barrier(
            command_buffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_ACCESS_HOST_READ_BIT
        );
    }

    template<class Vertex, class PointInstance>
    uint32_t convert_to_mesh(
        const PointCloud& point_cloud,
        VulkanBuffer& vertex_buffer,
        VulkanBuffer& index_buffer)
    {
        LOG_METHOD();

        {
            auto scope = m_command_buffer.begin_scope();
            convert_to_mesh<Vertex, PointInstance>(
                m_command_buffer,
                point_cloud,
                vertex_buffer,
                index_buffer
            );
        }
        submit_compute_commands();

        uint32_t valid_triangle_count = 0u;
        m_valid_triangle_count_buffer.read(&valid_triangle_count, sizeof(valid_triangle_count), 0u);
        return valid_triangle_count * 3u;
    }

    void submit_compute_commands();

private:
    VulkanQueue* m_queue = nullptr;

    VulkanCommandPool m_command_pool;
    VulkanCommandBuffer m_command_buffer;
    VulkanFence m_fence;

    PassWriter m_generate_mesh_pw;
    VulkanBuffer m_valid_triangle_count_buffer;

    uint32_t m_count_points_in_lidar_ring = 0;
};
