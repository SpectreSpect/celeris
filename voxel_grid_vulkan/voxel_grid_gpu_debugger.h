#pragma once

#include <glm/glm.hpp>
#include <functional>
#include <string>
#include <array>

class VulkanBuffer;
class VoxelGrid;
class Window;
class Camera;

class VoxelGridGPUDebugger {
public:
    VoxelGridGPUDebugger(VoxelGrid& voxel_grid);

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
    void display_build_from_dirty_window();
    void display_build_cmd_window(Window& window, const Camera& camera);
    void display_draw_pipline_window();
    void display_chunk_eviction_window(const Camera& camera);
    void display_stream_chunks_pipeline_window(const Camera& camera);
    void display_hash_table_window();

private:
    VoxelGrid* m_voxel_grid = nullptr;

    static constexpr int M_COUNT_DRAWING_STEPS = 3;
    std::string m_voxel_grid_draw_steps_names[M_COUNT_DRAWING_STEPS] = {
        "build_mesh_from_dirty()", 
        "build_indirect_draw_commands_frustum_fn()", 
        "draw_indirect()"};
    bool m_voxel_grid_draw_streaming[M_COUNT_DRAWING_STEPS] = {false};
    std::array<std::function<void()>, M_COUNT_DRAWING_STEPS> m_voxel_grid_draw_steps;
    
    static constexpr int M_COUNT_GENERATION_STEPS = 9; 
    bool m_voxel_grid_generation_streaming[M_COUNT_GENERATION_STEPS] = {false};
    std::array<std::function<void()>, M_COUNT_GENERATION_STEPS> m_voxel_grid_generation_steps;

    std::string m_voxel_grid_generation_steps_names[M_COUNT_GENERATION_STEPS] = {
        "ensure_free_chunks_gpu()",
        "reset_load_list_counter()",
        "mark_chunk_to_generate()",
        "merge_voxel_write_lists()",
        "reset_voxel_write_list_counter()",
        "mark_write_chunks_to_generate()",
        "generate_terrain()",
        "write_voxels_to_grid()",
        "reset_voxel_write_list_counter()"
    };
};