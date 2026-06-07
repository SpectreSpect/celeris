#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include <glm/glm.hpp>

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/vulkan_buffer.h"
#include "../vulkan_self/pass/instance/pass_instance.h"
#include "../vulkan_self/pass/instance/pass_writer.h"
#include "../vulkan_self/vulkan_command_buffer.h"
#include "../vulkan_self/vulkan_command_pool.h"
#include "../vulkan_self/vulkan_fence.h"
#include "voxel_grid_structures.h"
#include "shader_helper/shader_helper.h"

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
        VulkanDevice& device,
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

    // void apply_writes_to_world_gpu(uint32_t write_count);
    // void apply_writes_to_world_from_cpu(
    //     const std::vector<glm::ivec3>& positions,
    //     const std::vector<VoxelDataGPU>& voxels
    // );

    void update();

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
        VulkanBuffer load_list;
        VulkanBuffer local_voxel_write_list;
        VulkanBuffer voxel_write_list;
        VulkanBuffer voxels;

        VulkanBuffer bucket_heads;
        VulkanBuffer bucket_next;

        VulkanBuffer evicted_chunks_list;

        VulkanBuffer global_vertex_buffer;
        VulkanBuffer global_index_buffer;

        VulkanBuffer vb_heads;
        VulkanBuffer vb_nodes;
        VulkanBuffer vb_state;
        VulkanBuffer vb_free_nodes_list;

        VulkanBuffer ib_heads;
        VulkanBuffer ib_nodes;
        VulkanBuffer ib_state;
        VulkanBuffer ib_free_nodes_list;

        VulkanBuffer chunk_mesh_alloc;

        VulkanBuffer mesh_pool_clear_uniform;
        VulkanBuffer mesh_pool_seed_uniform;

        VulkanBuffer dispatch_args;

        VulkanBuffer dirty_quad_count;
        VulkanBuffer emit_counters;

        VulkanBuffer chunk_mesh_alloc_local;
        VulkanBuffer vb_returned_nodes_list;
        VulkanBuffer ib_returned_nodes_list;


        // vb_heads  vb_heads_ = BufferObject(sizeof(uint32_t) * (size_t)(vb_order_ + 1), GL_DYNAMIC_DRAW);
        // vb_state  vb_state_ = BufferObject(sizeof(uint32_t) * (size_t)count_vb_pages_, GL_DYNAMIC_DRAW);
        // vb_free_nodes_list   vb_free_nodes_list_ = BufferObject(sizeof(uint32_t) * (size_t)(1u + count_vb_nodes_), GL_DYNAMIC_DRAW);

        // ib_heads     ib_heads_ = BufferObject(sizeof(uint32_t) * (size_t)(ib_order_ + 1), GL_DYNAMIC_DRAW);
        // ib_state     ib_state_ = BufferObject(sizeof(uint32_t) * (size_t)count_ib_pages_, GL_DYNAMIC_DRAW);
        // ib_free_nodes_list   ib_free_nodes_list_ = BufferObject(sizeof(uint32_t) * (size_t)(1u + count_ib_nodes_), GL_DYNAMIC_DRAW);

        // chunk_mesh_alloc
    };

    struct VoxelGridPassInstances {
        PassWriter fill_buffer_pw;
        PassInstance world_init_pi;
        // PassInstance apply_writes_to_world_pi;
        PassInstance mesh_pool_clear_pi;
        PassInstance mesh_pool_seed_pi;
        PassInstance mesh_reset_pi;
        PassInstance mesh_count_pi;
        PassInstance mesh_alloc_vb_pi;
        PassInstance mesh_alloc_ib_pi;
        PassInstance verify_mesh_allocation_pi;
        PassWriter return_free_alloc_nodes_dispatch_adapter_pw;
        PassInstance return_free_alloc_nodes_pi;
        PassInstance mesh_emit_pi;
        PassInstance mesh_finalize_pi;
        PassInstance reset_dirty_count_pi;
        PassInstance stream_select_chunks_pi;
        PassWriter insert_elements_to_voxel_write_list_pw;
        PassWriter add_voxel_write_list_counters_together_pw;
        PassInstance mark_write_chunks_to_generate_pi;
        PassInstance stream_generate_terrain_pi;
        PassInstance write_voxels_to_grid_pi;
        PassInstance evict_buckets_build_pi;
        PassWriter evict_low_priority_dispatch_adapter_pw;
        PassInstance evict_low_priority_pi;
        PassInstance free_evicted_chunks_mesh_pi;
        PassInstance reset_evicted_list_and_buckets_pi;
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
    VulkanFence m_fence;

    VulkanQueue* m_queue = nullptr;
    ComputePassManager* m_compute_pass_manager = nullptr;

    VoxelGridParams m_params;
    VoxelGridPassInstances m_pass_instances;
    VoxelGridBuffers m_buffers;

    MeshView m_mesh_view;
    RenderObject m_render_object;

    ShaderHelper m_shader_helper;
    
private:
    uint64_t vox_per_chunk() const noexcept;

    VoxelGridParams create_params(const VoxelGridDesc& desc) const;
    VoxelGridPassInstances create_pass_instances(VulkanDevice& device, ComputePassManager& compute_pass_manager) const;
    VoxelGridBuffers create_buffers(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VulkanCommandBuffer& command_buffer
    );

    void insert_elements_to_voxel_write_list(
        VulkanCommandBuffer& command_buffer,
        const VulkanBuffer& dispatch_args,
        const VulkanBuffer& voxel_write_list_src,
        VulkanBuffer& voxel_write_list_dsc
    );
    void add_voxel_write_list_counters_together(
        VulkanCommandBuffer& command_buffer,
        const VulkanBuffer& voxel_write_list_src,
        VulkanBuffer& voxel_write_list_dsc
    );
    void merge_voxel_write_lists(VulkanCommandBuffer& command_buffer, const VulkanBuffer& voxel_write_list_src, VulkanBuffer& voxel_write_list_dsc);

    void world_init_gpu();
    // void init_draw_buffers();
    void init_mesh_pool();
    void submit_compute_commands();

    void reset_load_list_counter(VulkanCommandBuffer& command_buffer);
    void mark_chunk_to_generate(VulkanCommandBuffer& command_buffer, glm::vec3 cam_world_pos, int radius_chunks);
    void mark_write_chunks_to_generate(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args);
    void generate_terrain(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args, uint32_t seed);
    void write_voxels_to_grid(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args);
    void reset_voxel_write_list_counter(VulkanCommandBuffer& command_buffer, VulkanBuffer& voxel_write_list);
    void stream_chunks_sphere(VulkanCommandBuffer& command_buffer, glm::vec3 cam_world_pos, int radius_chunks, uint32_t seed);

    void reset_heads(VulkanCommandBuffer& command_buffer); 
    void build_bucket_lists(VulkanCommandBuffer& command_buffer, glm::vec3 cam_pos);
    void prepare_evict_lowpriority_chunks(VulkanCommandBuffer& command_buffer, VulkanBuffer& dispatch_args);
    void evict_lowpriority_chunks(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args);
    void free_evicted_chunks_mesh(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args);
    void reset_evicted_list_and_buckets(VulkanCommandBuffer& command_buffer);
    void ensure_free_chunks_gpu(VulkanCommandBuffer& command_buffer, glm::vec3 cam_pos, uint32_t pack_bits, uint32_t pack_offset);

    void mesh_reset(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args);
    void mesh_count(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args, uint32_t pack_bits, int32_t pack_offset); // not checked
    void mesh_alloc_vb(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args); // not checked
    void mesh_alloc_ib(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args); // not checked
    void mesh_alloc(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args); // not checked
    void verify_mesh_allocation(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args); // not checked
    void prepare_return_free_alloc_nodes(VulkanCommandBuffer& command_buffer, VulkanBuffer& dispatch_args); // not checked
    void return_free_alloc_nodes(VulkanCommandBuffer& command_buffer, VulkanBuffer& dispatch_args);
    void mesh_emit(VulkanCommandBuffer& command_buffer, VulkanBuffer& dispatch_args, uint32_t pack_bits, int32_t pack_offset);
    void mesh_finalize(VulkanCommandBuffer& command_buffer, VulkanBuffer& dispatch_args);
    void reset_dirty_count(VulkanCommandBuffer& command_buffer);
    void build_mesh_from_dirty(VulkanCommandBuffer& command_buffer, uint32_t pack_bits, int pack_offset);
};
