#pragma once

#include <cstdint>
#include <optional>
#include <glm/glm.hpp>

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/vulkan_buffer.h"
#include "../renderer/compute_pass_instance.h"
#include "../vulkan_self/buffer_filler.h"
#include "../vulkan_self/vulkan_command_buffer.h"
#include "../vulkan_self/vulkan_command_pool.h"

class VulkanPhysicalDevice;
class VulkanDevice;
class ComputePassManager;
class VulkanQueue;

class VoxelGrid {
public:
    _XCLASS_NAME(VoxelGrid);

    struct VoxelGridDesc {
        glm::ivec3 chunk_size; 
        glm::vec3 voxel_size;
        uint32_t count_active_chunks; 
        uint32_t max_quads;
        float chunk_hash_table_size_factor; 
        uint32_t count_evict_buckets;
        uint32_t min_free_chunks;
        float tomb_fraction_to_rebuild;
        float eviction_bucket_shell_thickness;
        uint32_t vb_page_size_order_of_two;
        uint32_t ib_page_size_order_of_two;
        float buddy_allocator_nodes_factor;
        float render_distance;
        uint32_t generation_distance;
        uint32_t max_write_count;
    };

public:
    VoxelGrid(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VulkanQueue& queue,
        ComputePassManager& compute_pass_manager,
        const VoxelGridDesc& desc
    );
    ~VoxelGrid() noexcept = default;

    VoxelGrid(const VoxelGrid&) = delete;
    VoxelGrid& operator=(const VoxelGrid&) = delete;

    VoxelGrid(VoxelGrid&&) noexcept = default;
    VoxelGrid& operator=(VoxelGrid&&) noexcept = default;

public:
    struct VoxelGridBuffers {
        VulkanBuffer chunk_hash_table;
        VulkanBuffer free_list;        
        VulkanBuffer chunk_meta;
        VulkanBuffer enqueued;
        VulkanBuffer indirect_cmds;
        VulkanBuffer failed_dirty_list;
        VulkanBuffer mesh_buffers_status;
        VulkanBuffer dirty_list;
        VulkanBuffer voxel_write_list;
    };

    struct VoxelGridPassInstances {
        ComputePassInstance fill_buffer_pi;
    };

    struct VoxelGridParams {
        glm::uvec3 chunk_size = {0u, 0u, 0u};
        glm::uvec3 voxel_size = {0u, 0u, 0u};
        uint32_t count_active_chunks = 0u;
        uint32_t count_evict_buckets = 0u;
        uint32_t max_write_count = 0u;
        uint32_t min_free_chunks = 0u;
        uint32_t chunk_hash_table_size = 0u;
        float tomb_fraction_to_rebuild = 0.0f;
        float eviction_bucket_shell_thickness = 0.0f;
        float render_distance = 0.0f;
        float generation_distance = 0.0f;

        uint32_t vb_page_size = 0;
        uint32_t count_vb_pages = 0;
        uint32_t count_vb_nodes = 0;
        uint32_t vb_order = 0;
        uint32_t max_mesh_vertices = 0;
        
        uint32_t ib_page_size = 0;
        uint32_t count_ib_pages = 0;
        uint32_t count_ib_nodes = 0;
        uint32_t ib_order = 0;
        uint32_t max_mesh_indices = 0;
    };

private:
    VulkanCommandPool m_command_pool;
    VulkanCommandBuffer m_command_buffer;

    ComputePassManager* m_compute_pass_manager = nullptr;

    BufferFiller m_buffer_filler;

    VoxelGridParams m_params;
    VoxelGridPassInstances m_pass_instances;
    VoxelGridBuffers m_buffers;
    
private:
    uint64_t vox_per_chunk() const noexcept;

    VoxelGridParams create_params(const VoxelGridDesc& desc) const;
    VoxelGridPassInstances create_pass_instances(ComputePassManager& compute_pass_manager) const;
    VoxelGridBuffers create_buffers(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VulkanCommandBuffer& command_buffer
    );

    void world_init_gpu();
};
