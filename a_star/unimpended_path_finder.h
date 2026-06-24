#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/vulkan_buffer.h"
#include "../vulkan_self/pass/instance/pass_instance.h"

class VulkanCommandBuffer;
class VulkanSubmitContext;
class VoxelGrid;
class VulkanPhysicalDevice;
class VulkanDevice;
class ComputePassManager;

class UnimpendedPathFinder {
public:
    _XCLASS_NAME(UnimpendedPathFinders);

    UnimpendedPathFinder(
        const VulkanPhysicalDevice& physical_device,
        VulkanDevice& device,
        VulkanSubmitContext& submit_context,
        ComputePassManager& compute_pass_manager,
        VoxelGrid& voxel_grid,
        uint32_t window_size,
        uint32_t max_astar_points
    );
    ~UnimpendedPathFinder() noexcept = default;

    UnimpendedPathFinder(const UnimpendedPathFinder&) = delete;
    UnimpendedPathFinder& operator=(const UnimpendedPathFinder&) = delete;

    UnimpendedPathFinder(UnimpendedPathFinder&&) noexcept = default;
    UnimpendedPathFinder& operator=(UnimpendedPathFinder&&) noexcept = default;

    void realloc_buffers(
        const VulkanPhysicalDevice& physical_device,
        VulkanDevice& device,
        VulkanSubmitContext& submit_context,
        uint32_t window_size,
        uint32_t max_astar_points
    );
    std::vector<glm::vec3> find_unimpended_path(
        const VulkanPhysicalDevice& physical_device,
        VulkanDevice& device,
        VulkanSubmitContext& submit_context, 
        std::vector<glm::vec4> astar_path,
        uint32_t max_step_height,
        uint32_t start_id = 0u
    );

struct FinderBuffers {
    VulkanBuffer max_unimpended_path_indices;
    VulkanBuffer astar_path;
};

struct FinderPassInstances {
    PassInstance find_unimpended_paths_pi;
};

private:
    VoxelGrid* m_voxel_grid;

    uint32_t m_window_size = 0u;
    uint32_t m_max_astar_points = 0u;

    FinderPassInstances m_pass_instances;
    FinderBuffers m_buffers;

private:
    FinderPassInstances create_pass_instances(VulkanDevice& device, ComputePassManager& compute_pass_manager) const;
    FinderBuffers create_buffers(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VulkanSubmitContext& submit_context
    ) const;

    void fill_max_unimpended_path_indices(
        VulkanCommandBuffer& command_buffer,
        uint32_t count_astar_points,
        uint32_t max_step_height,
        uint32_t start_id = 0u
    );

    std::vector<glm::vec3> build_path_from_max_unimpended_path_indices(
        std::vector<glm::vec4> astar_path
    );
};
