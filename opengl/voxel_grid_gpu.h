#pragma once
#define NOMINMAX
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <unordered_set>
#include <fstream>
#include <set>
#include <algorithm>
#include <limits>

#include "buffer_object.h"
#include "shader_manager.h"
#include "compute_program.h"
#include "vf_program.h"
#include "math_utils.h"
#include "transformable.h"
#include "drawable.h"
#include "vao.h"
#include "path_utils.h"
#include "buffer_dispatch_arg.h"
#include "value_dispatch_arg.h"
#include "gpu_timestamp.h"
#include "gridable_gpu.h"
#include "voxel_engine_gpu_structures.h"
#include "shader_helper.h"

class VoxelGridGPU : public Transformable, public Drawable, public IGridableGPU {
public:
    glm::ivec3 chunk_size;
    uint32_t count_active_chunks;
    glm::vec3 voxel_size;
    uint32_t chunk_hash_table_size;
    uint32_t count_evict_buckets;
    uint32_t min_free_chunks;
    float tomb_fraction_to_rebuild;
    uint32_t eviction_bucket_shell_thickness;
    uint32_t vox_per_chunk;
    float render_distance;
    uint32_t generation_distance;

    const uint32_t min_free_pages = 1024;

    struct PushLoopData {
        uint32_t old_h;
        uint32_t next_val;
        uint32_t old_value_of_next_val;
        uint32_t new_tag;
        uint32_t new_head;
        uint32_t was_head;
    };

    struct PushData {
        uint32_t input_start_page;
        uint32_t input_order;
        uint32_t old_state_of_input_start_page;
        uint32_t count_loop_cycles;
        PushLoopData loop_data[32];
    };

    struct MergeData {
        uint32_t cur_start;
        uint32_t cur_order;
        uint32_t cur_state;
        uint32_t buddy_size;
        uint32_t start_buddy;
        uint32_t buddy_state;
    };

    struct DebugStackElement {
        uint32_t dirty_idx;
        uint32_t chunk_id;
        uint32_t a_i_startPage; 
        uint32_t i_order;
        uint32_t prev_alloc_state;
        uint32_t count_merges;
        MergeData merge_data[33];
        uint32_t result_start;
        uint32_t result_order;
        uint32_t state_before_change;
        uint32_t state_after_change;
        PushData push_data;
        uint32_t current_head;
        uint32_t bool_push_result;
    };

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

    ShaderHelper* shader_helper = nullptr;
    ShaderManager* shader_manager = nullptr;

    ComputeProgram prog_dispatch_adapter_;
    ComputeProgram prog_clear_chunks_;
    ComputeProgram prog_world_init_;
    ComputeProgram prog_apply_writes_;
    ComputeProgram prog_mesh_reset_;
    ComputeProgram prog_mesh_count_;
    ComputeProgram prog_mesh_alloc_;
    ComputeProgram prog_mesh_emit_;
    ComputeProgram prog_mesh_finalize_;
    ComputeProgram prog_cmdcount_reset_;
    ComputeProgram prog_build_indirect_cmds_;
    ComputeProgram prog_reset_dirty_count_;
    ComputeProgram prog_evict_buckets_build_;
    ComputeProgram prog_evict_low_priority_;
    ComputeProgram prog_evict_low_priority_dispatch_adapter_;
    ComputeProgram prog_stream_select_chunks_;
    ComputeProgram prog_stream_generate_terrain_;
    ComputeProgram prog_mark_all_user_chunks_as_dirty_;
    ComputeProgram prog_mesh_pool_clear_;
    ComputeProgram prog_mesh_pool_seed_;
    ComputeProgram prog_verify_mesh_allocation_;
    ComputeProgram prog_return_free_alloc_nodes_;
    ComputeProgram prog_return_free_alloc_nodes_dispatch_adapter_;
    ComputeProgram prog_free_evicted_chunks_mesh_;
    ComputeProgram prog_fill_chunk_hash_table_;
    ComputeProgram prog_clear_chunk_hash_table_;
    ComputeProgram prog_reset_evicted_list_and_buckets_;
    ComputeProgram prog_hash_table_conditional_dispatch_adapter_;
    ComputeProgram prog_write_voxels_to_grid_;
    ComputeProgram prog_mark_write_chunks_to_generate_;
    ComputeProgram prog_insert_elements_to_voxel_write_list_;
    ComputeProgram prog_add_voxel_write_list_counters_together_;
    VfProgram prog_vf_voxel_mesh_diffusion_spec_;

    BufferObject chunk_hash_table_;
    BufferObject dispatch_args;
    BufferObject dispatch_args_additional;
    BufferObject voxels_;
    BufferObject chunk_meta_;
    BufferObject free_list_;
    BufferObject mesh_buffers_status_;
    BufferObject local_voxel_write_list_;
    BufferObject voxel_write_list_;
    BufferObject enqueued_;
    BufferObject dirty_list_;
    BufferObject global_vertex_buffer_;
    BufferObject global_index_buffer_;
    BufferObject voxel_prifab_;
    BufferObject dirty_quad_count_;
    BufferObject emit_counters_;
    BufferObject indirect_cmds_;
    BufferObject bucket_heads_;
    BufferObject bucket_next_;
    BufferObject load_list_;
    BufferObject failed_dirty_list_;
    BufferObject verify_debug_stack_;
    BufferObject evicted_chunks_list_;

    BufferObject vb_heads_;
    BufferObject vb_state_;
    BufferObject vb_nodes_;
    BufferObject vb_free_nodes_list_;
    BufferObject vb_returned_nodes_list;

    BufferObject ib_heads_;
    BufferObject ib_state_;
    BufferObject ib_nodes_;
    BufferObject ib_free_nodes_list_;
    BufferObject ib_returned_nodes_list;

    BufferObject chunk_mesh_alloc_local_;
    BufferObject chunk_mesh_alloc_;

    uint32_t vb_page_size_ = 0;
    uint32_t count_vb_pages_ = 0;
    uint32_t count_vb_nodes_ = 0;
    uint32_t vb_order_ = 0;
    uint32_t max_mesh_vertices_ = 0;
    
    uint32_t ib_page_size_ = 0;
    uint32_t count_ib_pages_ = 0;
    uint32_t count_ib_nodes_ = 0;
    uint32_t ib_order_ = 0;
    uint32_t max_mesh_indices_ = 0;

    VAO vao;

    VoxelGridGPU(const VoxelGridDesc& desc, ShaderHelper* shader_helper, ShaderManager* shader_manager);

    void apply_writes_to_world_gpu(uint32_t write_count);
    void apply_writes_to_world_from_cpu(const std::vector<glm::ivec3>& positions, const std::vector<VoxelDataGPU>& voxels);

    void apply_writes_to_world_gpu_with_evict(uint32_t write_count, const glm::vec3& cam_pos);
    void apply_writes_to_world_from_cpu_with_evict(const std::vector<glm::ivec3>& positions, const std::vector<VoxelDataGPU>& voxels, 
                                                    const glm::vec3& cam_pos);
    void reset_load_list_counter();
    void mark_chunk_to_generate(const glm::vec3& cam_world_pos, int radius_chunks);
    void mark_write_chunks_to_generate(const BufferObject& dispatch_args);
    void generate_terrain(const BufferObject& dispatch_args, uint32_t seed);
    void write_voxels_to_grid();
    void reset_voxel_write_list_counter(BufferObject& voxel_write_list);
    void stream_chunks_sphere(const glm::vec3& cam_world_pos, int radius_chunks, uint32_t seed);
    
    virtual void draw(RenderState state) override;
    void insert_elements_to_voxel_write_list(const BufferObject& dispatch_args, const BufferObject& voxel_write_list_src, BufferObject& voxel_write_list_dsc);
    void add_voxel_write_list_counters_together(const BufferObject& voxel_write_list_src, BufferObject& voxel_write_list_dsc);
    void merge_voxel_write_lists(const BufferObject& voxel_write_list_src, BufferObject& voxel_write_list_dsc);
    virtual void set_voxels(const BufferObject& voxel_write_list_src) override;
    virtual void set_voxels(const std::vector<Voxel>& voxels, const std::vector<glm::ivec3>& positions) override;
    virtual void set_voxel(const Voxel& voxel, glm::ivec3 position) override;
    virtual Voxel get_voxel(glm::ivec3 position) const override;

    void init_programs(ShaderManager& shader_manager);
    void world_init_gpu();
    void init_mesh_pool();

    void clear_chunk_hash_table(const BufferObject& dispatch_args); 
    void fill_chunk_hash_table(const BufferObject& dispatch_args, uint32_t pack_bits, uint32_t pack_offset); 
    void conditional_prepare_rebuild(BufferObject& clear_dispatch_args, BufferObject& fill_dispatch_args);
    void rebuild_chunk_hash_table(uint32_t pack_bits, uint32_t pack_offset);

    void reset_heads(); 
    void build_bucket_lists(const glm::vec3& cam_pos);
    void prepare_evict_lowpriority_chunks(const BufferObject& dispatch_args); 
    void evict_lowpriority_chunks(const BufferObject& dispatch_args); 
    void free_evicted_chunks_mesh(const BufferObject& dispatch_args); 
    void reset_evicted_list_and_buckets();
    void ensure_free_chunks_gpu(const glm::vec3& cam_pos, uint32_t pack_bits, uint32_t pack_offset); 

    void mesh_reset(const BufferObject& dispatch_args); 
    void mesh_count(const BufferObject& dispatch_args, uint32_t pack_bits, uint32_t pack_offset); 
    void mesh_alloc_vb(const BufferObject& dispatch_args); 
    void mesh_alloc_ib(const BufferObject& dispatch_args); 
    void mesh_alloc(const BufferObject& dispatch_args); 
    void verify_mesh_allocation(const BufferObject& dispatch_args); 
    void prepare_return_free_alloc_nodes(BufferObject& dispatch_args); 
    void return_free_alloc_nodes(const BufferObject& dispatch_args); 
    void mesh_emit(const BufferObject& dispatch_args, uint32_t pack_bits, uint32_t pack_offset); 
    void mesh_finalize(const BufferObject& dispatch_args); 
    void reset_dirty_count(); 
    void build_mesh_from_dirty(uint32_t pack_bits, int pack_offset); 

    void reset_cmd_count();
    void build_draw_commands(const glm::mat4& view_proj, const glm::vec3& cam_pos, uint32_t pack_bits, int pack_offset); 
    void build_indirect_draw_commands_frustum(const glm::mat4& viewProj, const glm::vec3& cam_pos, uint32_t pack_bits, int pack_offset); 

    void draw_indirect(const GLuint vao, const glm::mat4& world, const glm::mat4& proj_view, const glm::vec3& cam_pos); 

    void init_draw_buffers(); 

    void mark_all_used_chunks_as_dirty(); // Говно медленное 
};