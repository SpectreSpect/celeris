#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <vector>
#include <stdexcept>
#include <iostream>

#include "gridable_gpu.h"
#include "mesh.h"
#include "mesh_data.h"
#include "vertex_layout.h"
#include "buffer_object.h"
#include "compute_shader.h"
#include "compute_program.h"
#include "math_utils.h"
#include "shader_manager.h"
#include "voxel_engine_gpu_structures.h"
#include "shader_helper.h"
#include "gpu_timestamp.h"

class VoxelRasterizatorGPU {
public:
    IGridable* gridable = nullptr;
    IGridableGPU* gridable_gpu = nullptr;
    ShaderManager* shader_manager = nullptr;
    ShaderHelper* shader_helper = nullptr;

    glm::ivec3 chunk_size; 
    glm::vec3 voxel_size;
    uint32_t counter_hash_table_size;
    uint32_t count_voxel_writes;

    struct VoxelRasterizatorDesc {
        glm::ivec3 chunk_size;
        glm::vec3 voxel_size;
        uint32_t counter_hash_table_size;
        uint32_t count_voxel_writes;
    };

    VoxelRasterizatorGPU(const VoxelRasterizatorDesc& desc, IGridable* gridable, ShaderManager* shader_manager, ShaderHelper* shader_helper);
    ~VoxelRasterizatorGPU();

    void rasterize(const Mesh& mesh, const VoxelWriteGPU& prifab, BufferObject* out_voxel_writes = nullptr);

private:
    ComputeProgram prog_reset_voxelize_pipeline_;
    ComputeProgram prog_mark_and_count_active_chunks_;
    ComputeProgram prog_alloc_active_chunk_triangles_;
    ComputeProgram prog_fill_triangle_indices_;
    ComputeProgram prog_voxelize_triangles_;

    BufferObject dispatch_args;
    BufferObject counter_hash_table_;
    BufferObject active_chunk_keys_list_;
    BufferObject triangle_indices_list_;
    BufferObject voxel_writes_;
    
private:
    void init_programs();

    void reset_voxelize_pipline(BufferObject& voxel_writes, bool reset_voxel_write_list = true);
    void mark_and_count_active_chunks(const Mesh& mesh);
    void alloc_active_chunk_triangles(const BufferObject& dispatch_args);
    void fill_triangle_indices(const Mesh& mesh);
    void voxelize_chunks(
        const BufferObject& dispatch_args,
        const Mesh& mesh,
        BufferObject& voxel_writes,
        uint32_t voxel_type_flags,
        uint32_t voxel_color,
        uint32_t voxel_set_flags
    );

    // ===== DEBUG =====
    void print_triangle_emmit_diff();
    void print_hash_table_counters_state();
    void print_count_nonzero_triangles(const Mesh& mesh);
    void print_active_chunks();
    void print_total_count_triangles_in_hash_table();
    void print_triangle_list();
};
