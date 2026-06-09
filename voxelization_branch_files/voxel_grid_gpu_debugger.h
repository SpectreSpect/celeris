#pragma once
#define NOMINMAX
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <set>
#include <filesystem>
#include <array>
#include <sstream>
#include <iomanip>
#include <string>

#include "buffer_object.h"
#include "voxel_grid_gpu.h"
#include "imgui_layer.h"

class VoxelGridGPUDebugger {
public:
    VoxelGridGPU* voxel_grid = nullptr;
    Window* window = nullptr;
    ShaderHelper* shader_helper = nullptr;

    static constexpr int COUNT_DRAWING_STEPS = 3;
    bool voxel_grid_draw_streaming[COUNT_DRAWING_STEPS] = {false};
    std::array<std::function<void()>, COUNT_DRAWING_STEPS> voxel_grid_draw_steps;
    std::string voxel_grid_draw_steps_names[COUNT_DRAWING_STEPS] = {"build_mesh_from_dirty()", "build_indirect_draw_commands_frustum_fn()", "draw_indirect()"};

    static constexpr int COUNT_GENERATION_STEPS = 9; 
    bool voxel_grid_generation_streaming[COUNT_GENERATION_STEPS] = {false};
    std::array<std::function<void()>, COUNT_GENERATION_STEPS> voxel_grid_generation_steps;

    std::string voxel_grid_generation_steps_names[COUNT_GENERATION_STEPS] = {
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

    VoxelGridGPUDebugger(VoxelGridGPU* voxel_grid, ShaderHelper* shader_helper, Window* window);

    void print_finded_chunks_in_hash_table(glm::ivec3 chunk_pos);

    void print_counters();
    void print_count_free_mesh_alloc();
    void print_chunks_hash_table_log(); 
    void print_eviction_log(const glm::vec3& camera_pos); 

    void print_dirty_list();
    void print_dirty_list_emit_counters();
    void print_dirty_list_quad_count();
    void print_mesh_alloc_by_dirty_list(const std::string& prefix, uint32_t mesh_alloc_start_page_offset_bytes, uint32_t mesh_alloc_order_offset_bytes);

    void print_free_lists(
        const BufferObject& heads_buffer,
        const BufferObject& nodes_buffer,
        const BufferObject& states_buffer,
        uint32_t count_nodes,
        uint32_t count_pages,
        uint32_t max_order
    );

    void dispay_debug_window();
    void display_build_from_dirty_window();
    void display_build_cmd_window();
    void display_draw_pipline_window();
    void display_chunk_eviction_window();
    void display_stream_chunks_pipeline_window();
    void display_hash_table_window();
};