#pragma once

#include <vector>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <functional>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>


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
#include "../renderer/indirect_render_object.h"
#include "../renderer/mesh_view.h"

class VulkanPhysicalDevice;
class VulkanDevice;
class ComputePassManager;
class MaterialInstanceManager;
class VulkanQueue;
class Camera;
class Window;
class PointCloud;


class VoxelGridChunk {
public:
    _XCLASS_NAME(VoxelGridChunk);

    VoxelGridChunk() = default;
    VoxelGridChunk(glm::uvec3 chunk_size, std::vector<VoxelDataGPU> voxels);

    glm::uvec3 chunk_size() const noexcept;
    uint32_t voxel_count() const noexcept;

    const VoxelDataGPU& voxel(uint32_t x, uint32_t y, uint32_t z) const;
    VoxelDataGPU& voxel(uint32_t x, uint32_t y, uint32_t z);

    const VoxelDataGPU& voxel(glm::uvec3 local_pos) const;
    VoxelDataGPU& voxel(glm::uvec3 local_pos);

    const std::vector<VoxelDataGPU>& voxels() const noexcept;
    std::vector<VoxelDataGPU>& voxels() noexcept;

private:
    uint32_t voxel_index(uint32_t x, uint32_t y, uint32_t z) const;

    glm::uvec3 m_chunk_size{0u, 0u, 0u};
    std::vector<VoxelDataGPU> m_voxels;
};


class VoxelGrid {
public:
    _XCLASS_NAME(VoxelGrid);

    friend class VoxelGridGPUDebugger;

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
        uint32_t mean_count_quads_in_chunk;
        uint32_t allocation_retry_list_size;
        float buddy_allocator_nodes_factor;
        float render_distance;
        uint32_t generation_distance;
        uint32_t max_write_count;
    };

    struct BuddyAllocatorParams {
        uint32_t page_size = 0;
        uint32_t count_pages = 0;
        uint32_t count_nodes = 0;
        uint32_t max_order = 0;
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

        BuddyAllocatorParams vb_allocator_params;
        BuddyAllocatorParams ib_allocator_params;

        uint32_t allocation_retry_list_size = 0;
        uint32_t count_allocation_retry_attempts = 10;
    };

    struct AllocatorBuffers {
        VulkanBuffer heads;
        VulkanBuffer nodes;
        VulkanBuffer state;
        VulkanBuffer free_nodes_list;
        VulkanBuffer returned_nodes_list;
    };

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

        VulkanBuffer active_splitters;

        AllocatorBuffers vb_mesh_allocator_buffers;
        AllocatorBuffers ib_mesh_allocator_buffers;

        VulkanBuffer allocation_retry_list;
        VulkanBuffer allocation_retry_list_additional;

        VulkanBuffer chunk_mesh_alloc;

        VulkanBuffer mesh_pool_clear_uniform;
        VulkanBuffer mesh_pool_seed_uniform;

        VulkanBuffer dispatch_args;
        VulkanBuffer dispatch_args_additional;

        VulkanBuffer dirty_quad_count;
        VulkanBuffer emit_counters;

        VulkanBuffer chunk_mesh_alloc_local;

        VulkanBuffer build_indirect_cmds_uniform;
        VulkanBuffer read_chunk_output;
        VulkanBuffer check_footprint_result;
        VulkanBuffer read_and_inflate_chunk_output;

        VulkanBuffer debug_counter;
    };

public:
    VoxelGrid(
        const VulkanPhysicalDevice& physical_device,
        VulkanDevice& device,
        VulkanQueue& queue,
        ComputePassManager& compute_pass_manager,
        MaterialInstanceManager& material_instance_manager,
        const VoxelGridDesc& desc,
        VkMemoryPropertyFlags ssbo_memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    ~VoxelGrid() noexcept = default;

    VoxelGrid(const VoxelGrid&) = delete;
    VoxelGrid& operator=(const VoxelGrid&) = delete;

    VoxelGrid(VoxelGrid&&) noexcept = default;
    VoxelGrid& operator=(VoxelGrid&&) noexcept = default;

    IndirectRenderObject& render_object();
    VulkanBuffer& local_voxel_write_list() noexcept;
    const VoxelGridParams& params() const noexcept;
    VoxelGridBuffers& buffers() noexcept;
    ShaderHelper& shader_helper() noexcept;


    glm::vec3 voxel_size();
    void voxelize_point_cloud(VulkanCommandBuffer& command_buffer, VulkanEngine& engine, 
                              PointCloud& point_cloud, VulkanBuffer& voxel_writes, uint32_t max_write_count);
    void voxelize_point_cloud(VulkanEngine& engine, PointCloud& point_cloud, 
                              VulkanBuffer& voxel_writes, uint32_t max_write_count);
    
    void update(Window& window, Camera& camera);
    void set_voxels(VulkanCommandBuffer& command_buffer, const VulkanBuffer& voxel_write_list_src);
    void set_render_distance(float value);
    VoxelGridChunk read_chunk(glm::ivec3 chunk_pos);
    bool check_footprint(glm::vec3 origin, glm::vec3 offsets, uint32_t max_step_up);
    std::vector<VoxelGridChunk> read_and_inflate_chunk(glm::ivec3 chunk_pos, uint32_t inflation_size);
    glm::ivec3 chunk_pos_from_voxel_pos(glm::ivec3 voxel_pos);
    glm::ivec3 pos_in_chunk_from_global_voxel_pos(glm::ivec3 voxel_pos);
    glm::ivec3 pos_in_chunk_from_global_voxel_pos(glm::ivec3 chunk_pos, glm::ivec3 voxel_pos);

    void add_next_to_stream_chunks_sphere_callback(std::function<void(VulkanCommandBuffer&, VoxelGrid&)> callback);
    void add_next_to_update_submit_callbacks(std::function<void(VoxelGrid&)> callback);

public:
    struct VoxelGridPassInstances {
        PassWriter fill_buffer_pw;
        PassInstance world_init_pi;
        PassInstance mesh_pool_clear_pi;
        PassInstance mesh_pool_seed_pi;
        PassInstance mesh_reset_pi;
        PassInstance mesh_count_pi;
        PassWriter mesh_alloc_pw;
        PassWriter retry_mesh_alloc_pw;
        PassInstance verify_mesh_allocation_pi;
        PassWriter return_free_alloc_nodes_dispatch_adapter_pw;
        PassWriter return_free_alloc_nodes_pw;
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
        PassInstance build_indirect_cmds_pi;
        PassInstance free_evicted_chunks_mesh_pi;
        PassInstance reset_evicted_list_and_buckets_pi;
        PassWriter hash_table_conditional_dispatch_adapter_pw;
        PassInstance clear_chunk_hash_table_pi;
        PassInstance fill_chunk_hash_table_pi;
        PassInstance read_voxel_grid_chunk_pi;
        PassInstance check_footprint_pi;
        PassInstance read_and_inflate_voxel_grid_chunk_pi;
        
        PassInstance voxel_writes_from_point_cloud_pi;
    };

private:
    VulkanCommandPool m_command_pool;
    VulkanCommandBuffer m_command_buffer;
    VulkanFence m_fence;

    std::mutex m_compute_mutex;

    VulkanQueue* m_queue = nullptr;

    VoxelGridParams m_params;
    VoxelGridPassInstances m_pass_instances;
    VoxelGridBuffers m_buffers;

    MeshView m_mesh_view;
    IndirectRenderObject m_render_object;

    ShaderHelper m_shader_helper;

    std::vector<std::function<void(VulkanCommandBuffer&, VoxelGrid&)>> m_next_to_stream_chunks_sphere_callbacks;
    std::vector<std::function<void(VoxelGrid&)>> m_next_to_update_submit_callbacks;
    
private:
    uint64_t vox_per_chunk() const noexcept;

    VoxelGridParams create_params(const VoxelGridDesc& desc) const;
    VoxelGridPassInstances create_pass_instances(VulkanDevice& device, ComputePassManager& compute_pass_manager) const;
    VoxelGridBuffers create_buffers(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VulkanCommandBuffer& command_buffer,
        VkMemoryPropertyFlags ssbo_memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
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
    void init_mesh_pool();
    void submit_compute_commands();
    
    void reset_load_list_counter(VulkanCommandBuffer& command_buffer);
    void mark_chunk_to_generate(VulkanCommandBuffer& command_buffer, glm::vec3 cam_world_pos, int radius_chunks);
    void mark_write_chunks_to_generate(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args);
    void generate_terrain(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args, uint32_t seed);
    void write_voxels_to_grid(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args);
    void reset_voxel_write_list_counter(VulkanCommandBuffer& command_buffer, VulkanBuffer& voxel_write_list);
    void stream_chunks_sphere(VulkanCommandBuffer& command_buffer, glm::vec3 cam_world_pos, int radius_chunks, uint32_t seed);
    
    void conditional_prepare_rebuild(VulkanCommandBuffer& command_buffer, VulkanBuffer& clear_dispatch_args, VulkanBuffer& fill_dispatch_args);
    void clear_chunk_hash_table(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args);
    void fill_chunk_hash_table(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args, uint32_t pack_bits, int pack_offset);
    void rebuild_chunk_hash_table(VulkanCommandBuffer& command_buffer, uint32_t pack_bits, int pack_offset);

    void reset_heads(VulkanCommandBuffer& command_buffer); 
    void build_bucket_lists(VulkanCommandBuffer& command_buffer, glm::vec3 cam_pos);
    void prepare_evict_lowpriority_chunks(VulkanCommandBuffer& command_buffer, VulkanBuffer& dispatch_args);
    void evict_lowpriority_chunks(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args);
    void free_evicted_chunks_mesh(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args);
    void reset_evicted_list_and_buckets(VulkanCommandBuffer& command_buffer);
    void ensure_free_chunks_gpu(VulkanCommandBuffer& command_buffer, glm::vec3 cam_pos, uint32_t pack_bits, int pack_offset);

    void mesh_reset(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args);
    void mesh_count(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args, uint32_t pack_bits, int pack_offset);

    void reset_allocation_retry_list(VulkanCommandBuffer& command_buffer, VulkanBuffer& allocation_retry_list);
    void mesh_alloc_buffer(
        VulkanCommandBuffer& command_buffer, 
        const VulkanBuffer& dispatch_args,
        AllocatorBuffers& mesh_allocator_buffers,
        BuddyAllocatorParams& mesh_allocator_params,
        uint32_t quad_size,
        bool is_vertex_phase
    );
    void retry_mesh_alloc(
        VulkanCommandBuffer& command_buffer,
        const VulkanBuffer& dispatch_args,
        VulkanBuffer& readable_retry_list,
        VulkanBuffer& writable_retry_list,
        AllocatorBuffers& mesh_allocator_buffers,
        BuddyAllocatorParams& mesh_allocator_params,
        uint32_t quad_size,
        bool is_vertex_phase
    );
    void mesh_alloc(
        VulkanCommandBuffer& command_buffer,
        VulkanBuffer& dispatch_args,
        AllocatorBuffers& mesh_allocator_buffers,
        BuddyAllocatorParams& mesh_allocator_params,
        uint32_t quad_size,
        bool is_vertex_phase
    );

    void verify_mesh_allocation(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args);
    void prepare_return_free_alloc_nodes(VulkanCommandBuffer& command_buffer, VulkanBuffer& dispatch_args);
    void return_free_alloc_nodes(VulkanCommandBuffer& command_buffer, VulkanBuffer& dispatch_args);
    void mesh_emit(VulkanCommandBuffer& command_buffer, VulkanBuffer& dispatch_args, uint32_t pack_bits, int pack_offset);
    void mesh_finalize(VulkanCommandBuffer& command_buffer, VulkanBuffer& dispatch_args);
    void reset_dirty_count(VulkanCommandBuffer& command_buffer);
    void reset_cmd_count(VulkanCommandBuffer& command_buffer);
    void build_draw_commands(VulkanCommandBuffer& command_buffer, const glm::mat4& view_proj, const glm::vec3& cam_pos, uint32_t pack_bits, int pack_offset);

    void build_mesh_from_dirty(VulkanCommandBuffer& command_buffer, uint32_t pack_bits, int pack_offset);
    void build_indirect_draw_commands_frustum(VulkanCommandBuffer& command_buffer, 
        const glm::mat4& viewProj,
        const glm::vec3& cam_pos,
        uint32_t pack_bits,
        int pack_offset
    );
};
