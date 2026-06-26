#pragma once

#include <glm/glm.hpp>
#include <functional>
#include <string>
#include <array>

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/vulkan_command_buffer.h"
#include "../vulkan_self/vulkan_command_pool.h"
#include "../vulkan_self/vulkan_fence.h"

class VulkanBuffer;
class VoxelGrid;
class Window;
class Camera;
class VulkanQueue;
class VulkanDevice;

class VoxelGridGPUDebugger {
public:
    _XCLASS_NAME(VoxelGridGPUDebugger);

    VoxelGridGPUDebugger(
        VoxelGrid& voxel_grid,
        VulkanDevice& device,
        VulkanQueue& queue,
        const Window& window,
        const Camera& camera,
        bool default_tasks_activity_state
    );

    VulkanCommandBuffer& command_buffer() noexcept;

    void print_found_chunks_in_hash_table(glm::ivec3 chunk_pos);

    void print_counters();
    void print_count_free_mesh_alloc();
    void print_chunks_hash_table_log();
    void print_eviction_log(const glm::vec3& camera_pos); 

    void print_dirty_list();
    void print_dirty_list_emit_counters();
    void print_dirty_list_quad_count();
    void print_mesh_alloc_by_dirty_list(
        const std::string& prefix, 
        uint32_t mesh_alloc_start_page_offset_bytes, 
        uint32_t mesh_alloc_order_offset_bytes
    );

    void print_free_lists(
        VulkanBuffer& heads_buffer,
        VulkanBuffer& nodes_buffer,
        VulkanBuffer& states_buffer,
        uint32_t count_nodes,
        uint32_t count_pages,
        uint32_t max_order
    );

    void dispay_debug_window(const Camera& camera);
    void display_build_from_dirty_window(VulkanCommandBuffer& command_buffer);
    void display_build_cmd_window(VulkanCommandBuffer& command_buffer, Window& window, const Camera& camera);
    void display_draw_pipline_window(VulkanCommandBuffer& command_buffer);
    void display_chunk_eviction_window(VulkanCommandBuffer& command_buffer, const Camera& camera);
    void display_stream_chunks_pipeline_window(VulkanCommandBuffer& command_buffer, const Camera& camera);
    void display_hash_table_window();

    void submit_commands();

private:
struct Task {
    std::string name;
    bool is_active;
    std::function<void(VulkanCommandBuffer&)> func;
};

private:
    VoxelGrid* m_voxel_grid = nullptr;

    VulkanQueue* m_queue = nullptr;
    
    VulkanCommandPool m_command_pool;
    VulkanCommandBuffer m_command_buffer;
    VulkanFence m_fence;
    

    std::vector<Task> m_draw_tasks;
    std::vector<Task> m_generation_tasks; 

    std::vector<Task> create_draw_tasks(
        const Window& window,
        const Camera& camera,
        bool default_tasks_activity_state = true
    );
    std::vector<Task> create_generation_tasks(
        const Camera& camera,
        bool default_tasks_activity_state = true
    );
};
