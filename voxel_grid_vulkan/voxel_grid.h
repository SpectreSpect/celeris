#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include <glm/glm.hpp>

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/vulkan_buffer.h"
#include "../vulkan_self/pass/instance/pass_instance.h"
#include "../vulkan_self/buffer_filler.h"
#include "../vulkan_self/vulkan_command_buffer.h"
#include "../vulkan_self/vulkan_command_pool.h"
#include "../vulkan_self/vulkan_fence.h"
#include "voxel_grid_structures.h"


#include "../renderer/render_object.h"
#include "../renderer/mesh_view.h"

class VulkanPhysicalDevice;
class VulkanDevice;
class ComputePassManager;
class MaterialInstanceManager;
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
        MaterialInstanceManager& material_instance_manager,
        const VoxelGridDesc& desc
    );
    ~VoxelGrid() noexcept = default;

    VoxelGrid(const VoxelGrid&) = delete;
    VoxelGrid& operator=(const VoxelGrid&) = delete;

    VoxelGrid(VoxelGrid&&) noexcept = default;
    VoxelGrid& operator=(VoxelGrid&&) noexcept = default;

    void apply_writes_to_world_gpu(uint32_t write_count);
    void apply_writes_to_world_from_cpu(
        const std::vector<glm::ivec3>& positions,
        const std::vector<VoxelDataGPU>& voxels
    );
    void build_mesh_from_dirty();
    void build_indirect_draw_commands_frustum(
        const glm::mat4& view_proj,
        const glm::vec3& camera_position
    );
    void render_indirect(Renderer& renderer, VulkanCommandBuffer& command_buffer, const glm::mat4& world_transform);
    void mark_all_used_chunks_as_dirty();
    void stream_chunks_sphere(const glm::vec3& camera_position, int radius_chunks, uint32_t seed);
    void ensure_free_chunks_gpu(const glm::vec3& camera_position);

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
        VulkanBuffer voxels;
        VulkanBuffer dirty_quad_count;
        VulkanBuffer emit_counters;

        VulkanBuffer global_vertex_buffer;
        VulkanBuffer global_index_buffer;

        VulkanBuffer vb_heads;
        VulkanBuffer vb_nodes;
        VulkanBuffer vb_state;
        VulkanBuffer vb_free_nodes_list;
        VulkanBuffer vb_returned_nodes_list;

        VulkanBuffer ib_heads;
        VulkanBuffer ib_nodes;
        VulkanBuffer ib_state;
        VulkanBuffer ib_free_nodes_list;
        VulkanBuffer ib_returned_nodes_list;

        VulkanBuffer chunk_mesh_alloc;
        VulkanBuffer chunk_mesh_alloc_local;

        VulkanBuffer mesh_pool_clear_uniform;
        VulkanBuffer mesh_pool_seed_uniform;
        VulkanBuffer build_indirect_uniform;
        VulkanBuffer load_list;
        VulkanBuffer bucket_heads;
        VulkanBuffer bucket_next;
        VulkanBuffer evicted_chunks_list;

        // vb_heads  vb_heads_ = BufferObject(sizeof(uint32_t) * (size_t)(vb_order_ + 1), GL_DYNAMIC_DRAW);
        // vb_state  vb_state_ = BufferObject(sizeof(uint32_t) * (size_t)count_vb_pages_, GL_DYNAMIC_DRAW);
        // vb_free_nodes_list   vb_free_nodes_list_ = BufferObject(sizeof(uint32_t) * (size_t)(1u + count_vb_nodes_), GL_DYNAMIC_DRAW);

        // ib_heads     ib_heads_ = BufferObject(sizeof(uint32_t) * (size_t)(ib_order_ + 1), GL_DYNAMIC_DRAW);
        // ib_state     ib_state_ = BufferObject(sizeof(uint32_t) * (size_t)count_ib_pages_, GL_DYNAMIC_DRAW);
        // ib_free_nodes_list   ib_free_nodes_list_ = BufferObject(sizeof(uint32_t) * (size_t)(1u + count_ib_nodes_), GL_DYNAMIC_DRAW);

        // chunk_mesh_alloc
    };

    struct VoxelGridPassInstances {
        PassInstance fill_buffer_pi;
        PassInstance world_init_pi;
        PassInstance apply_writes_to_world_pi;
        PassInstance mesh_pool_clear_pi;
        PassInstance mesh_pool_seed_pi;
        PassInstance mesh_reset_pi;
        PassInstance mesh_count_pi;
        PassInstance mesh_alloc_vertex_pi;
        PassInstance mesh_alloc_index_pi;
        PassInstance mesh_emit_pi;
        PassInstance mesh_finalize_pi;
        PassInstance verify_mesh_allocation_pi;
        PassInstance return_free_alloc_nodes_pi;
        PassInstance reset_dirty_count_pi;
        PassInstance build_indirect_cmds_pi;
        PassInstance mark_all_used_chunks_as_dirty_pi;
        PassInstance stream_select_chunks_pi;
        PassInstance stream_generate_terrain_pi;
        PassInstance evict_buckets_build_pi;
        PassInstance evict_low_priority_pi;
        PassInstance free_evicted_chunks_mesh_pi;
        PassInstance reset_evicted_list_and_buckets_pi;
        PassInstance clear_chunk_hash_table_pi;
        PassInstance fill_chunk_hash_table_pi;
    };

    struct VoxelGridParams {
        glm::uvec3 chunk_size = {0u, 0u, 0u};
        glm::vec3 voxel_size = {0.0f, 0.0f, 0.0f};
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
    VulkanFence m_fence;

    VulkanQueue* m_queue = nullptr;
    ComputePassManager* m_compute_pass_manager = nullptr;

    // BufferFiller m_buffer_filler;

    VoxelGridParams m_params;
    VoxelGridPassInstances m_pass_instances;
    VoxelGridBuffers m_buffers;

    MeshView m_mesh_view;
    RenderObject m_render_object;
    
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
    // void init_draw_buffers();
    void init_mesh_pool();
    void dispatch_mesh_alloc_phase(bool vertex_phase);
    void memory_barrier_compute_rw(VulkanBuffer& buffer);
    void reset_load_list_counter();
    void reset_bucket_heads();
    uint32_t free_chunk_count() const;
    void record_rebuild_chunk_hash_table();
    void submit_compute_commands();
};
