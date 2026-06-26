#pragma once

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/vulkan_buffer.h"
#include "../vulkan_self/pass/instance/pass_instance.h"

class VulkanCommandBuffer;
class VulkanSubmitContext;
class VoxelGrid;
class VulkanPhysicalDevice;
class VulkanDevice;
class ComputePassManager;

class PathIntersectionDetector {
public:
    _XCLASS_NAME(PathIntersectionDetector);

    PathIntersectionDetector(
        const VulkanPhysicalDevice& physical_device,
        VulkanDevice& device,
        VulkanSubmitContext& submit_context,
        ComputePassManager& compute_pass_manager,
        VoxelGrid& voxel_grid,
        uint32_t max_path_points
    );
    ~PathIntersectionDetector() noexcept = default;

    PathIntersectionDetector(const PathIntersectionDetector&) = delete;
    PathIntersectionDetector& operator=(const PathIntersectionDetector&) = delete;

    PathIntersectionDetector(PathIntersectionDetector&&) noexcept = default;
    PathIntersectionDetector& operator=(PathIntersectionDetector&&) noexcept = default;

    void realloc_buffers(
        VulkanSubmitContext& submit_context,
        uint32_t max_path_points
    );

    bool is_path_passable(
        VulkanSubmitContext& submit_context,
        const std::vector<glm::vec3>& path_points
    );

    bool has_intersection(
        VulkanSubmitContext& submit_context,
        const std::vector<glm::vec3>& path_points
    );

struct DetectorBuffers {
    VulkanBuffer path_points;
    VulkanBuffer path_passability;
};

struct DetectorPassInstances {
    PassInstance check_path_passability_pi;
};

private:
    VoxelGrid* m_voxel_grid = nullptr;
    const VulkanPhysicalDevice* m_physical_device = nullptr;
    VulkanDevice* m_device = nullptr;

    uint32_t m_max_path_points = 0u;

    DetectorPassInstances m_pass_instances;
    DetectorBuffers m_buffers;

private:
    DetectorPassInstances create_pass_instances(
        VulkanDevice& device,
        ComputePassManager& compute_pass_manager
    ) const;

    DetectorBuffers create_buffers(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VulkanSubmitContext& submit_context
    ) const;

    void check_path_passability(
        VulkanCommandBuffer& command_buffer,
        uint32_t count_path_points
    );
};
