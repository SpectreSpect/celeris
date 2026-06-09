#include "voxel_grid_gpu.h"

VoxelGridGPU::VoxelGridGPU(const VoxelGridDesc& desc, ShaderHelper* shader_helper, ShaderManager* shader_manager)
    :   chunk_size(desc.chunk_size),
        voxel_size(desc.voxel_size),
        count_active_chunks(desc.count_active_chunks),
        count_evict_buckets(desc.count_evict_buckets),
        min_free_chunks(desc.min_free_chunks),
        tomb_fraction_to_rebuild(desc.tomb_fraction_to_rebuild),
        eviction_bucket_shell_thickness(desc.eviction_bucket_shell_thickness),
        render_distance(desc.render_distance),
        generation_distance(desc.generation_distance),
        shader_manager(shader_manager),
        shader_helper(shader_helper)
 {
    if (desc.chunk_hash_table_size_factor < 1.0f) {
        std::string message = "VoxelGridGPU: chunk_hash_table_size_factor must be >= 1.0.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    vox_per_chunk = (uint32_t)(chunk_size.x * chunk_size.y * chunk_size.z);

    uint64_t raw = (uint64_t)std::ceil((double)desc.chunk_hash_table_size_factor * (double)count_active_chunks);
    uint32_t base = (raw > UINT32_MAX) ? UINT32_MAX : (uint32_t)raw;
    this->chunk_hash_table_size = math_utils::next_pow2_u32(base);

    if ((chunk_hash_table_size & (chunk_hash_table_size - 1)) != 0) {
        std::string message = "VoxelGridGPU: chunk_hash_table_size must be 2^n.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    init_programs(*shader_manager);

    dispatch_args = BufferObject::from_fill(sizeof(uint32_t) * 3u, GL_DYNAMIC_DRAW, 1u, *shader_manager);
    dispatch_args_additional = BufferObject::from_fill(sizeof(uint32_t) * 3u, GL_DYNAMIC_DRAW, 1u, *shader_manager);
    
    chunk_meta_ = BufferObject(sizeof(ChunkMetaGPU) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);
    free_list_ = BufferObject(sizeof(uint32_t) * (size_t)(1 + count_active_chunks), GL_DYNAMIC_DRAW);
    indirect_cmds_ = BufferObject(sizeof(uint32_t) + sizeof(DrawElementsIndirectCommand) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);

    mesh_buffers_status_ = BufferObject::from_fill(sizeof(uint32_t) * 2, GL_DYNAMIC_DRAW, 0u, *shader_manager);

    enqueued_ = BufferObject(sizeof(uint32_t) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);
    dirty_list_ = BufferObject::from_fill(sizeof(uint32_t) * (size_t)(1 + count_active_chunks), GL_DYNAMIC_DRAW, 0u, *shader_manager);
    failed_dirty_list_ = BufferObject(sizeof(uint32_t) * (size_t)(1 + count_active_chunks), GL_DYNAMIC_DRAW);

    dirty_quad_count_ = BufferObject(sizeof(uint32_t) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);
    emit_counters_     = BufferObject(sizeof(uint32_t) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);

    BucketHead bucket_head;
    bucket_head.id = INVALID_ID;
    bucket_head.count = 0;
    
    bucket_heads_ = BufferObject::from_fill(sizeof(BucketHead) * count_evict_buckets, GL_DYNAMIC_DRAW, bucket_head, *shader_manager);
    bucket_next_  = BufferObject::from_fill(sizeof(uint32_t) * count_active_chunks, GL_DYNAMIC_DRAW, INVALID_ID, *shader_manager);
    verify_debug_stack_ = BufferObject::from_fill(sizeof(uint32_t) * 2 + sizeof(DebugStackElement) * 10'000, GL_DYNAMIC_DRAW, INVALID_ID, *shader_manager);
    verify_debug_stack_.update_subdata_fill(0, 0u, sizeof(uint32_t) * 2, *shader_manager);

    evicted_chunks_list_ = BufferObject::from_fill(sizeof(uint32_t) * (count_active_chunks + 1), GL_DYNAMIC_DRAW, 0u, *shader_manager);
    voxel_write_list_ = BufferObject::from_fill(sizeof(uint32_t) * 4 + sizeof(VoxelWriteGPU) * desc.max_write_count, GL_DYNAMIC_DRAW, 0u, *shader_manager);
    local_voxel_write_list_ = BufferObject::from_fill(sizeof(uint32_t) * 4 + sizeof(VoxelWriteGPU) * desc.max_write_count, GL_DYNAMIC_DRAW, 0u, *shader_manager);

    vb_page_size_ = 1 << desc.vb_page_size_order_of_two;
    count_vb_pages_ = math_utils::next_pow2_u32(math_utils::div_up_u32((desc.max_quads * 4u), vb_page_size_));
    count_vb_nodes_ = ceil(count_vb_pages_ * desc.buddy_allocator_nodes_factor);
    vb_order_ = math_utils::log2_floor_u32(count_vb_pages_);
    max_mesh_vertices_ = count_vb_pages_ * vb_page_size_;
    
    std::cout << "vb_page_size_: " << vb_page_size_ << std::endl;
    std::cout << "count_vb_pages_: " << count_vb_pages_ << std::endl;
    std::cout << "count_vb_nodes_: " << count_vb_nodes_ << std::endl;
    std::cout << "vb_order_: " << vb_order_ << std::endl;
    std::cout << "max_mesh_vertices_: " << max_mesh_vertices_ << std::endl;
    std::cout << std::endl;

    global_vertex_buffer_ = BufferObject(sizeof(VertexGPU) * (size_t)max_mesh_vertices_, GL_DYNAMIC_DRAW);
    vb_heads_ = BufferObject(sizeof(uint32_t) * (size_t)(vb_order_ + 1), GL_DYNAMIC_DRAW);
    vb_nodes_ = BufferObject(sizeof(AllocNode) * (size_t)(count_vb_nodes_), GL_DYNAMIC_DRAW);
    vb_free_nodes_list_ = BufferObject(sizeof(uint32_t) * (size_t)(1u + count_vb_nodes_), GL_DYNAMIC_DRAW);
    vb_returned_nodes_list = BufferObject::from_fill(sizeof(uint32_t) * (size_t)(1u + count_vb_nodes_), GL_DYNAMIC_DRAW, 0u, *shader_manager);
    vb_state_ = BufferObject(sizeof(uint32_t) * (size_t)count_vb_pages_, GL_DYNAMIC_DRAW);

    ib_page_size_ = 1 << desc.ib_page_size_order_of_two;
    count_ib_pages_ = math_utils::next_pow2_u32(math_utils::div_up_u32((desc.max_quads * 6u), ib_page_size_));
    count_ib_nodes_ = ceil(count_ib_pages_ * desc.buddy_allocator_nodes_factor);
    ib_order_ = math_utils::log2_floor_u32(count_ib_pages_);
    max_mesh_indices_ = count_ib_pages_ * ib_page_size_;

    std::cout << "ib_page_size_: " << ib_page_size_ << std::endl;
    std::cout << "count_ib_pages_: " << count_ib_pages_ << std::endl;
    std::cout << "count_ib_nodes_: " << count_ib_nodes_ << std::endl;
    std::cout << "ib_order_: " << ib_order_ << std::endl;
    std::cout << "max_mesh_indices_: " << max_mesh_indices_ << std::endl;
    std::cout << std::endl;
    
    global_index_buffer_ = BufferObject(sizeof(uint32_t) * (size_t)max_mesh_indices_, GL_DYNAMIC_DRAW);
    ib_heads_ = BufferObject(sizeof(uint32_t) * (size_t)(ib_order_ + 1), GL_DYNAMIC_DRAW);
    ib_nodes_ = BufferObject(sizeof(AllocNode) * (size_t)(count_ib_nodes_), GL_DYNAMIC_DRAW);
    ib_free_nodes_list_ = BufferObject(sizeof(uint32_t) * (size_t)(1u + count_ib_nodes_), GL_DYNAMIC_DRAW);
    ib_returned_nodes_list = BufferObject::from_fill(sizeof(uint32_t) * (size_t)(1u + count_ib_nodes_), GL_DYNAMIC_DRAW, 0u, *shader_manager);
    ib_state_ = BufferObject(sizeof(uint32_t) * (size_t)count_ib_pages_, GL_DYNAMIC_DRAW);

    chunk_mesh_alloc_ = BufferObject(sizeof(ChunkMeshAlloc) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);
    chunk_mesh_alloc_local_ = BufferObject(sizeof(ChunkMeshAlloc) * (size_t)count_active_chunks, GL_DYNAMIC_DRAW);

    voxel_prifab_ = BufferObject(sizeof(VoxelDataGPU), GL_DYNAMIC_DRAW);

    load_list_ = BufferObject(sizeof(uint32_t) * (size_t)(1 + count_active_chunks), GL_DYNAMIC_DRAW);

    VoxelDataGPU voxel_prifab(0u, VOXEL_VISABILITY_FLAG_BIT, glm::ivec3(255));
    uint32_t count_voxels_in_chunk = chunk_size.x * chunk_size.y * chunk_size.z;
    voxels_ = BufferObject::from_fill(sizeof(VoxelDataGPU) * count_voxels_in_chunk * count_active_chunks, GL_DYNAMIC_DRAW, voxel_prifab, *shader_manager);

    // alignof(ChunkHashTableSlot) == 8!!!
    chunk_hash_table_ = BufferObject(sizeof(HashTableCounters) + sizeof(ChunkHashTableSlot) * chunk_hash_table_size, GL_DYNAMIC_DRAW);

    world_init_gpu();
    init_draw_buffers();
    init_mesh_pool();
}

void VoxelGridGPU::apply_writes_to_world_gpu(uint32_t write_count) {
    voxel_write_list_.update_subdata(0u, sizeof(uint32_t), &write_count);

    chunk_hash_table_.bind_base_as_ssbo(0);
    voxel_write_list_.bind_base_as_ssbo(1);
    voxels_.bind_base_as_ssbo(2);
    free_list_.bind_base_as_ssbo(3);
    chunk_meta_.bind_base_as_ssbo(4);
    enqueued_.bind_base_as_ssbo(5);
    dirty_list_.bind_base_as_ssbo(6);

    prog_apply_writes_.use();

    glUniform1ui(glGetUniformLocation(prog_apply_writes_.id, "u_chunk_hash_table_size"), chunk_hash_table_size);
    glUniform3i(glGetUniformLocation(prog_apply_writes_.id, "u_chunk_dim"),
                chunk_size.x, chunk_size.y, chunk_size.z);
    glUniform1ui(glGetUniformLocation(prog_apply_writes_.id, "u_voxels_per_chunk"), vox_per_chunk);
    glUniform1ui(glGetUniformLocation(prog_apply_writes_.id, "u_pack_bits"), math_utils::BITS);
    glUniform1i(glGetUniformLocation(prog_apply_writes_.id, "u_pack_offset"), math_utils::OFFSET);


    uint32_t groups_x = math_utils::div_up_u32(write_count, 256u);
    prog_apply_writes_.dispatch_compute(groups_x, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::apply_writes_to_world_from_cpu(const std::vector<glm::ivec3>& positions, const std::vector<VoxelDataGPU>& voxels) {
    assert(positions.size() == voxels.size());

    std::vector<VoxelWriteGPU> voxel_writes;
    voxel_writes.resize(positions.size());

    for (size_t i = 0; i < positions.size(); i++) {
        voxel_writes[i].world_voxel = glm::ivec4(positions[i].x, positions[i].y, positions[i].z, 1);
        voxel_writes[i].voxel_data = voxels[i];
    }

    voxel_write_list_.update_subdata(0u, sizeof(VoxelWriteGPU) * voxel_writes.size(), voxel_writes.data());

    apply_writes_to_world_gpu(voxel_writes.size());
}

void VoxelGridGPU::apply_writes_to_world_gpu_with_evict(uint32_t write_count, const glm::vec3& cam_pos) {
    ensure_free_chunks_gpu(cam_pos, math_utils::BITS, math_utils::OFFSET);
    apply_writes_to_world_gpu(write_count);
}

void VoxelGridGPU::apply_writes_to_world_from_cpu_with_evict(
    const std::vector<glm::ivec3>& positions, 
    const std::vector<VoxelDataGPU>& voxels,
    const glm::vec3& cam_pos) {
    ensure_free_chunks_gpu(cam_pos, math_utils::BITS, math_utils::OFFSET);
    apply_writes_to_world_from_cpu(positions, voxels);
}

void VoxelGridGPU::reset_load_list_counter() {
    load_list_.update_subdata_fill<uint32_t>(0u, 0u, sizeof(uint32_t), *shader_manager);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::mark_chunk_to_generate(const glm::vec3& cam_world_pos, int radius_chunks)
{
    glm::mat4 invM = glm::inverse(get_model_matrix());
    glm::vec3 cam_local = glm::vec3(invM * glm::vec4(cam_world_pos, 1.0f));

    chunk_hash_table_.bind_base_as_ssbo(0);
    free_list_.bind_base_as_ssbo(1);
    chunk_meta_.bind_base_as_ssbo(2);
    enqueued_.bind_base_as_ssbo(3);
    load_list_.bind_base_as_ssbo(4);

    prog_stream_select_chunks_.use();
    glUniform1ui(glGetUniformLocation(prog_stream_select_chunks_.id, "u_chunk_hash_table_size"), chunk_hash_table_size);
    glUniform1ui(glGetUniformLocation(prog_stream_select_chunks_.id, "u_max_load_entries"), count_active_chunks);
    glUniform3i(glGetUniformLocation(prog_stream_select_chunks_.id, "u_chunk_dim"), chunk_size.x, chunk_size.y, chunk_size.z);
    glUniform3f(glGetUniformLocation(prog_stream_select_chunks_.id, "u_voxel_size"), voxel_size.x, voxel_size.y, voxel_size.z);
    glUniform3f(glGetUniformLocation(prog_stream_select_chunks_.id, "u_cam_pos_local"), cam_local.x, cam_local.y, cam_local.z);
    glUniform1i(glGetUniformLocation(prog_stream_select_chunks_.id, "u_radius_chunks"), radius_chunks);
    glUniform1ui(glGetUniformLocation(prog_stream_select_chunks_.id, "u_pack_bits"), math_utils::BITS);
    glUniform1i(glGetUniformLocation(prog_stream_select_chunks_.id, "u_pack_offset"), math_utils::OFFSET);

    const uint32_t side = (uint32_t)(2 * radius_chunks + 1);
    const uint32_t gx = math_utils::div_up_u32(side, 8u);
    const uint32_t gy = math_utils::div_up_u32(side, 8u);
    const uint32_t gz = math_utils::div_up_u32(side, 8u);

    prog_stream_select_chunks_.dispatch_compute(gx, gy, gz);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::mark_write_chunks_to_generate(const BufferObject& dispatch_args) {
    voxel_write_list_.bind_base_as_ssbo(0);
    load_list_.bind_base_as_ssbo(1);
    chunk_hash_table_.bind_base_as_ssbo(2);
    free_list_.bind_base_as_ssbo(3);
    chunk_meta_.bind_base_as_ssbo(4);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_args.id());

    prog_mark_write_chunks_to_generate_.use();

    glUniform3ui(glGetUniformLocation(prog_mark_write_chunks_to_generate_.id, "u_chunk_dim"), chunk_size.x, chunk_size.y, chunk_size.z);
    glUniform1ui(glGetUniformLocation(prog_mark_write_chunks_to_generate_.id, "u_chunk_hash_table_size"), chunk_hash_table_size);
    glUniform1ui(glGetUniformLocation(prog_mark_write_chunks_to_generate_.id, "u_pack_offset"), math_utils::OFFSET);
    glUniform1ui(glGetUniformLocation(prog_mark_write_chunks_to_generate_.id, "u_pack_bits"), math_utils::BITS);

    glDispatchComputeIndirect(0);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::generate_terrain(const BufferObject& dispatch_args, uint32_t seed) {
    chunk_hash_table_.bind_base_as_ssbo(0);

    load_list_.bind_base_as_ssbo(1);

    voxels_.bind_base_as_ssbo(2);
    chunk_meta_.bind_base_as_ssbo(3);
    enqueued_.bind_base_as_ssbo(4);
    dirty_list_.bind_base_as_ssbo(5);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_args.id());

    prog_stream_generate_terrain_.use();
    glUniform3i(glGetUniformLocation(prog_stream_generate_terrain_.id, "u_chunk_dim"), chunk_size.x, chunk_size.y, chunk_size.z);
    glUniform1ui(glGetUniformLocation(prog_stream_generate_terrain_.id, "u_voxels_per_chunk"), vox_per_chunk);
    glUniform1ui(glGetUniformLocation(prog_stream_generate_terrain_.id, "u_pack_bits"), math_utils::BITS);
    glUniform1i(glGetUniformLocation(prog_stream_generate_terrain_.id, "u_pack_offset"), math_utils::OFFSET);
    glUniform1ui(glGetUniformLocation(prog_stream_generate_terrain_.id, "u_seed"), seed);
    glUniform1ui(glGetUniformLocation(prog_stream_generate_terrain_.id, "u_chunk_hash_table_size"), chunk_hash_table_size);

    glDispatchComputeIndirect(0);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::write_voxels_to_grid() {
    chunk_hash_table_.bind_base_as_ssbo(0);
    free_list_.bind_base_as_ssbo(1);
    chunk_meta_.bind_base_as_ssbo(2);
    enqueued_.bind_base_as_ssbo(3);
    dirty_list_.bind_base_as_ssbo(4);
    voxel_write_list_.bind_base_as_ssbo(5);
    voxels_.bind_base_as_ssbo(6);
    
    prog_write_voxels_to_grid_.use();

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_args.id());

    glUniform1ui(glGetUniformLocation(prog_write_voxels_to_grid_.id, "u_chunk_hash_table_size"), chunk_hash_table_size);
    glUniform3i(glGetUniformLocation(prog_write_voxels_to_grid_.id, "u_chunk_dim"), chunk_size.x, chunk_size.y, chunk_size.z);
    glUniform1ui(glGetUniformLocation(prog_write_voxels_to_grid_.id, "u_voxels_per_chunk"), vox_per_chunk);

    glUniform1ui(glGetUniformLocation(prog_write_voxels_to_grid_.id, "u_pack_offset"), math_utils::OFFSET);
    glUniform1ui(glGetUniformLocation(prog_write_voxels_to_grid_.id, "u_pack_bits"), math_utils::BITS);

    glDispatchComputeIndirect(0);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::reset_voxel_write_list_counter(BufferObject& voxel_write_list) {
    voxel_write_list.update_subdata_fill<uint32_t>(0, 0u, sizeof(uint32_t), *shader_manager);
}

void VoxelGridGPU::stream_chunks_sphere(const glm::vec3& cam_world_pos, int radius_chunks, uint32_t seed) {
    if (radius_chunks < 0) radius_chunks = generation_distance;

    ensure_free_chunks_gpu(cam_world_pos, math_utils::BITS, math_utils::OFFSET);

    reset_load_list_counter();

    mark_chunk_to_generate(cam_world_pos, radius_chunks);

    merge_voxel_write_lists(local_voxel_write_list_, voxel_write_list_);

    reset_voxel_write_list_counter(local_voxel_write_list_);
    
    shader_helper->prepare_dispatch_args(dispatch_args, BufferDispatchArg(&voxel_write_list_, 0));
    mark_write_chunks_to_generate(dispatch_args);

    shader_helper->prepare_dispatch_args(dispatch_args, ValueDispatchArg(vox_per_chunk), BufferDispatchArg(&load_list_, 0u));
    generate_terrain(dispatch_args, seed);

    shader_helper->prepare_dispatch_args(dispatch_args, BufferDispatchArg(&voxel_write_list_, 0u));
    write_voxels_to_grid();

    reset_voxel_write_list_counter(voxel_write_list_);
}


void VoxelGridGPU::draw(RenderState state) {
    state.transform *= get_model_matrix();

    build_mesh_from_dirty(math_utils::BITS, math_utils::OFFSET);
    build_indirect_draw_commands_frustum(state.vp, state.camera->position, math_utils::BITS, math_utils::OFFSET);
    draw_indirect(vao.id, state.transform, state.vp, state.camera->position);
}

void VoxelGridGPU::insert_elements_to_voxel_write_list(const BufferObject& dispatch_args, const BufferObject& voxel_write_list_src, BufferObject& voxel_write_list_dsc) {
    voxel_write_list_src.bind_base_as_ssbo(0);
    voxel_write_list_dsc.bind_base_as_ssbo(1);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_args.id());

    prog_insert_elements_to_voxel_write_list_.use();
    glDispatchComputeIndirect(0);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::add_voxel_write_list_counters_together(const BufferObject& voxel_write_list_src, BufferObject& voxel_write_list_dsc) {
    voxel_write_list_src.bind_base_as_ssbo(0);
    voxel_write_list_dsc.bind_base_as_ssbo(1);

    prog_add_voxel_write_list_counters_together_.use();
    prog_add_voxel_write_list_counters_together_.dispatch_compute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::merge_voxel_write_lists(const BufferObject& voxel_write_list_src, BufferObject& voxel_write_list_dsc) {
    shader_helper->prepare_dispatch_args(dispatch_args, BufferDispatchArg(&voxel_write_list_src, 0));
    insert_elements_to_voxel_write_list(dispatch_args, voxel_write_list_src, voxel_write_list_dsc);
    add_voxel_write_list_counters_together(voxel_write_list_src, voxel_write_list_dsc);
}

void VoxelGridGPU::set_voxels(const BufferObject& voxel_write_list_src) {
    merge_voxel_write_lists(voxel_write_list_src, local_voxel_write_list_);
}

void VoxelGridGPU::set_voxels(const std::vector<Voxel>& voxels, const std::vector<glm::ivec3>& positions) {

}

void VoxelGridGPU::set_voxel(const Voxel& voxel, glm::ivec3 position) {

}

Voxel VoxelGridGPU::get_voxel(glm::ivec3 position) const {
    return Voxel();
}

void VoxelGridGPU::init_programs(ShaderManager& shader_manager) {
    prog_clear_chunks_ = ComputeProgram(&shader_manager.clear_chunks_cs);
    prog_dispatch_adapter_ = ComputeProgram(&shader_manager.dispatch_adapter_cs);
    prog_world_init_ = ComputeProgram(&shader_manager.world_init_cs);
    prog_apply_writes_ = ComputeProgram(&shader_manager.apply_writes_to_world_cs);
    prog_mesh_reset_ = ComputeProgram(&shader_manager.mesh_reset_cs);
    prog_mesh_count_ = ComputeProgram(&shader_manager.mesh_count_cs);
    prog_mesh_alloc_ = ComputeProgram(&shader_manager.mesh_alloc_cs);
    prog_mesh_emit_ = ComputeProgram(&shader_manager.mesh_emit_cs);
    prog_mesh_finalize_ = ComputeProgram(&shader_manager.mesh_finalize_cs);
    prog_build_indirect_cmds_ = ComputeProgram(&shader_manager.build_indirect_cmds_cs);
    prog_reset_dirty_count_ = ComputeProgram(&shader_manager.reset_dirty_count_cs);
    prog_evict_buckets_build_ = ComputeProgram(&shader_manager.evict_buckets_build_cs);
    prog_evict_low_priority_ = ComputeProgram(&shader_manager.evict_low_priority_cs);
    prog_evict_low_priority_dispatch_adapter_ = ComputeProgram(&shader_manager.evict_low_priority_dispatch_adapter_cs);
    prog_stream_select_chunks_ = ComputeProgram(&shader_manager.stream_select_chunks_cs);
    prog_stream_generate_terrain_ = ComputeProgram(&shader_manager.stream_generate_terrain_cs);
    prog_mark_all_user_chunks_as_dirty_ = ComputeProgram(&shader_manager.mark_all_user_chunks_as_dirty_cs);
    prog_mesh_pool_clear_ = ComputeProgram(&shader_manager.mesh_pool_clear_cs);
    prog_mesh_pool_seed_ = ComputeProgram(&shader_manager.mesh_pool_seed_cs);
    prog_verify_mesh_allocation_ = ComputeProgram(&shader_manager.verify_mesh_allocation_cs);
    prog_return_free_alloc_nodes_ = ComputeProgram(&shader_manager.return_free_alloc_nodes_cs);
    prog_return_free_alloc_nodes_dispatch_adapter_ = ComputeProgram(&shader_manager.return_free_alloc_nodes_dispatch_adapter_cs);
    prog_free_evicted_chunks_mesh_ = ComputeProgram(&shader_manager.free_evicted_chunks_mesh_cs);
    prog_fill_chunk_hash_table_ = ComputeProgram(&shader_manager.fill_chunk_hash_table_cs);
    prog_clear_chunk_hash_table_ = ComputeProgram(&shader_manager.clear_chunk_hash_table_cs);
    prog_reset_evicted_list_and_buckets_ = ComputeProgram(&shader_manager.reset_evicted_list_and_buckets_cs);
    prog_hash_table_conditional_dispatch_adapter_ = ComputeProgram(&shader_manager.hash_table_conditional_dispatch_adapter_cs);
    prog_write_voxels_to_grid_ = ComputeProgram(&shader_manager.write_voxels_to_grid_cs);
    prog_mark_write_chunks_to_generate_ = ComputeProgram(&shader_manager.mark_write_chunks_to_generate_cs);
    prog_insert_elements_to_voxel_write_list_ = ComputeProgram(&shader_manager.insert_elements_to_voxel_write_list_cs);
    prog_add_voxel_write_list_counters_together_ = ComputeProgram(&shader_manager.add_voxel_write_list_counters_together_cs);
    prog_vf_voxel_mesh_diffusion_spec_ = VfProgram(&shader_manager.voxel_mesh_vs, &shader_manager.voxel_mesh_fs);
}

void VoxelGridGPU::world_init_gpu() {
    chunk_hash_table_.bind_base_as_ssbo(0);
    free_list_.bind_base_as_ssbo(1);
    mesh_buffers_status_.bind_base_as_ssbo(2);
    chunk_meta_.bind_base_as_ssbo(3);
    enqueued_.bind_base_as_ssbo(4);
    dirty_list_.bind_base_as_ssbo(5);
    voxel_write_list_.bind_base_as_ssbo(6);
    indirect_cmds_.bind_base_as_ssbo(7);
    failed_dirty_list_.bind_base_as_ssbo(8);

    prog_world_init_.use();
    glUniform1ui(glGetUniformLocation(prog_world_init_.id, "u_chunk_hash_table_size"), chunk_hash_table_size);
    glUniform1ui(glGetUniformLocation(prog_world_init_.id, "u_max_chunks"), count_active_chunks);

    uint32_t maxItems = std::max(chunk_hash_table_size, count_active_chunks);
    uint32_t groups_x = math_utils::div_up_u32(maxItems, 256u);

    prog_world_init_.dispatch_compute(groups_x, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::init_mesh_pool() {
    // Pass 1: mesh_pool_clear
    vb_heads_.bind_base_as_ssbo(0);
    vb_state_.bind_base_as_ssbo(1);
    vb_free_nodes_list_.bind_base_as_ssbo(2);

    ib_heads_.bind_base_as_ssbo(3);
    ib_state_.bind_base_as_ssbo(4);
    ib_free_nodes_list_.bind_base_as_ssbo(5);

    chunk_mesh_alloc_.bind_base_as_ssbo(6);

    prog_mesh_pool_clear_.use();

    glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_vb_pages"), count_vb_pages_);
    glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_ib_pages"), count_ib_pages_);
    glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_vb_nodes"), count_vb_nodes_);
    glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_ib_nodes"), count_ib_nodes_);
    glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_vb_heads_count"), vb_order_ + 1);
    glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_ib_heads_count"), ib_order_ + 1);
    glUniform1ui(glGetUniformLocation(prog_mesh_pool_clear_.id, "u_max_chunks"), count_active_chunks);

    uint32_t max_count = std::max({count_vb_pages_, count_ib_pages_, count_active_chunks, count_vb_nodes_, count_ib_nodes_});
    uint32_t groups_x = math_utils::div_up_u32(max_count, 256u);
    prog_mesh_pool_clear_.dispatch_compute(groups_x, 1, 1);
    
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Pass 2: mesh_pool_seed
    vb_heads_.bind_base_as_ssbo(0);
    vb_nodes_.bind_base_as_ssbo(1);
    vb_state_.bind_base_as_ssbo(2);
    vb_free_nodes_list_.bind_base_as_ssbo(3);

    ib_heads_.bind_base_as_ssbo(4);
    ib_nodes_.bind_base_as_ssbo(5);
    ib_state_.bind_base_as_ssbo(6);
    ib_free_nodes_list_.bind_base_as_ssbo(7);

    prog_mesh_pool_seed_.use();

    glUniform1ui(glGetUniformLocation(prog_mesh_pool_seed_.id, "u_vb_max_order"), vb_order_);
    glUniform1ui(glGetUniformLocation(prog_mesh_pool_seed_.id, "u_ib_max_order"), ib_order_);

    prog_mesh_pool_seed_.dispatch_compute(1, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::clear_chunk_hash_table(const BufferObject& dispatch_args) {
    chunk_hash_table_.bind_base_as_ssbo(0);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_args.id());

    prog_clear_chunk_hash_table_.use();
    glUniform1ui(glGetUniformLocation(prog_clear_chunk_hash_table_.id, "u_chunk_hash_table_size"), chunk_hash_table_size);
    
    glDispatchComputeIndirect(0);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::fill_chunk_hash_table(const BufferObject& dispatch_args, uint32_t pack_bits, uint32_t pack_offset) {
    chunk_hash_table_.bind_base_as_ssbo(0);
    chunk_meta_.bind_base_as_ssbo(1);
    enqueued_.bind_base_as_ssbo(2);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_args.id());

    prog_fill_chunk_hash_table_.use();
    glUniform1ui(glGetUniformLocation(prog_fill_chunk_hash_table_.id, "u_max_chunks"), count_active_chunks);
    glUniform1ui(glGetUniformLocation(prog_fill_chunk_hash_table_.id, "u_chunk_hash_table_size"), chunk_hash_table_size);
    glUniform1ui(glGetUniformLocation(prog_fill_chunk_hash_table_.id, "u_pack_bits"), pack_bits);
    glUniform1ui(glGetUniformLocation(prog_fill_chunk_hash_table_.id, "u_pack_offset"), pack_offset);

    glDispatchComputeIndirect(0);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::conditional_prepare_rebuild(BufferObject& clear_dispatch_args, BufferObject& fill_dispatch_args) {
    chunk_hash_table_.bind_base_as_ssbo(0);
    clear_dispatch_args.bind_base_as_ssbo(1);
    fill_dispatch_args.bind_base_as_ssbo(2);
    
    uint32_t tombs_to_rebuild = (uint32_t)(tomb_fraction_to_rebuild * chunk_hash_table_size);

    prog_hash_table_conditional_dispatch_adapter_.use();
    glUniform1ui(glGetUniformLocation(prog_hash_table_conditional_dispatch_adapter_.id, "u_chunk_hash_table_size"), chunk_hash_table_size);
    glUniform1ui(glGetUniformLocation(prog_hash_table_conditional_dispatch_adapter_.id, "u_max_chunks"), count_active_chunks);
    glUniform1ui(glGetUniformLocation(prog_hash_table_conditional_dispatch_adapter_.id, "u_tombs_to_rebuild"), tombs_to_rebuild);

    prog_hash_table_conditional_dispatch_adapter_.dispatch_compute(1, 1, 1);

    glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::rebuild_chunk_hash_table(uint32_t pack_bits, uint32_t pack_offset) {
    conditional_prepare_rebuild(dispatch_args, dispatch_args_additional);

    clear_chunk_hash_table(dispatch_args);
    fill_chunk_hash_table(dispatch_args_additional, pack_bits, pack_offset);
}

void VoxelGridGPU::reset_heads() {
    BucketHead bucket_head;
    bucket_head.id = INVALID_ID;
    bucket_head.count = 0;

    bucket_heads_.update_subdata_fill<BucketHead>(0u, bucket_head, sizeof(BucketHead) * count_evict_buckets, *shader_manager);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::build_bucket_lists(const glm::vec3& cam_pos) {
    chunk_meta_.bind_base_as_ssbo(0);
    bucket_heads_.bind_base_as_ssbo(1);
    bucket_next_.bind_base_as_ssbo(2);

    prog_evict_buckets_build_.use();
    glUniform1ui(glGetUniformLocation(prog_evict_buckets_build_.id, "u_max_chunks"), count_active_chunks);
    glUniform1ui(glGetUniformLocation(prog_evict_buckets_build_.id, "u_bucket_count"), this->count_evict_buckets);
    glUniform3f(glGetUniformLocation(prog_evict_buckets_build_.id, "u_cam_pos"), cam_pos.x, cam_pos.y, cam_pos.z);
    glUniform3i(glGetUniformLocation(prog_evict_buckets_build_.id, "u_chunk_dim"), chunk_size.x, chunk_size.y, chunk_size.z);
    glUniform3f(glGetUniformLocation(prog_evict_buckets_build_.id, "u_voxel_size"), voxel_size.x, voxel_size.y, voxel_size.z);

    glUniform1ui(glGetUniformLocation(prog_evict_buckets_build_.id, "u_pack_bits"), math_utils::BITS);
    glUniform1i(glGetUniformLocation(prog_evict_buckets_build_.id, "u_pack_offset"), math_utils::OFFSET);

    glUniform1f(glGetUniformLocation(prog_evict_buckets_build_.id, "f_eviction_bucket_shell_thickness"), eviction_bucket_shell_thickness);

    uint32_t gx = math_utils::div_up_u32(count_active_chunks, 256u);
    prog_evict_buckets_build_.dispatch_compute(gx, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::prepare_evict_lowpriority_chunks(const BufferObject& dispatch_args) {
    evicted_chunks_list_.bind_base_as_ssbo(0);
    dispatch_args.bind_base_as_ssbo(1);
    free_list_.bind_base_as_ssbo(2);
    
    prog_evict_low_priority_dispatch_adapter_.use();
    glUniform1ui(glGetUniformLocation(prog_evict_low_priority_dispatch_adapter_.id, "u_min_free"), min_free_chunks);
    
    prog_evict_low_priority_dispatch_adapter_.dispatch_compute(1, 1, 1);
    glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::evict_lowpriority_chunks(const BufferObject& dispatch_args) {
    chunk_hash_table_.bind_base_as_ssbo(0);
    free_list_.bind_base_as_ssbo(1);
    chunk_meta_.bind_base_as_ssbo(2);
    enqueued_.bind_base_as_ssbo(3);
    bucket_heads_.bind_base_as_ssbo(4);
    bucket_next_.bind_base_as_ssbo(5);
    chunk_mesh_alloc_.bind_base_as_ssbo(6);
    evicted_chunks_list_.bind_base_as_ssbo(7);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_args.id());

    prog_evict_low_priority_.use();
    glUniform1ui(glGetUniformLocation(prog_evict_low_priority_.id, "u_chunk_hash_table_size"), chunk_hash_table_size);
    glUniform1ui(glGetUniformLocation(prog_evict_low_priority_.id, "u_bucket_count"), count_evict_buckets);

    glDispatchComputeIndirect(0);

    glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::free_evicted_chunks_mesh(const BufferObject& dispatch_args) {
    chunk_mesh_alloc_.bind_base_as_ssbo(0);

    vb_heads_.bind_base_as_ssbo(1);
    vb_state_.bind_base_as_ssbo(2);
    vb_nodes_.bind_base_as_ssbo(3);
    vb_free_nodes_list_.bind_base_as_ssbo(4);
    vb_returned_nodes_list.bind_base_as_ssbo(5);

    ib_heads_.bind_base_as_ssbo(6);
    ib_state_.bind_base_as_ssbo(7);
    ib_nodes_.bind_base_as_ssbo(8);
    ib_free_nodes_list_.bind_base_as_ssbo(9);
    ib_returned_nodes_list.bind_base_as_ssbo(10);

    evicted_chunks_list_.bind_base_as_ssbo(11);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_args.id());

    prog_free_evicted_chunks_mesh_.use();
    glUniform1ui(glGetUniformLocation(prog_free_evicted_chunks_mesh_.id, "vb_max_order"), vb_order_);
    glUniform1ui(glGetUniformLocation(prog_free_evicted_chunks_mesh_.id, "ib_max_order"), ib_order_);

    glDispatchComputeIndirect(0);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::reset_evicted_list_and_buckets() {
    bucket_heads_.bind_base_as_ssbo(0);
    evicted_chunks_list_.bind_base_as_ssbo(1);
    free_list_.bind_base_as_ssbo(2);

    prog_reset_evicted_list_and_buckets_.use();
    glUniform1ui(glGetUniformLocation(prog_reset_evicted_list_and_buckets_.id, "u_bucket_count"), count_evict_buckets);

    uint32_t bucket_count_groups = math_utils::div_up_u32(count_evict_buckets, 256u);
    prog_reset_evicted_list_and_buckets_.dispatch_compute(bucket_count_groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::ensure_free_chunks_gpu(const glm::vec3& cam_pos, uint32_t pack_bits, uint32_t pack_offset) {
    reset_heads();

    build_bucket_lists(cam_pos);
    
    prepare_evict_lowpriority_chunks(dispatch_args);
    evict_lowpriority_chunks(dispatch_args);
    
    free_evicted_chunks_mesh(dispatch_args); // dispatch_args здесь уже подготовлен

    reset_evicted_list_and_buckets();

    prepare_return_free_alloc_nodes(dispatch_args);
    return_free_alloc_nodes(dispatch_args);

    rebuild_chunk_hash_table(pack_bits, pack_offset);
}

void VoxelGridGPU::mesh_reset(const BufferObject& dispatch_args) {
    dirty_list_.bind_base_as_ssbo(0);
    dirty_quad_count_.bind_base_as_ssbo(1);
    emit_counters_.bind_base_as_ssbo(2);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_args.id());

    prog_mesh_reset_.use();
    glDispatchComputeIndirect(0);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::mesh_count(const BufferObject& dispatch_args, uint32_t pack_bits, uint32_t pack_offset) {
    chunk_hash_table_.bind_base_as_ssbo(0);
    voxels_.bind_base_as_ssbo(1);
    dirty_list_.bind_base_as_ssbo(2);
    dirty_quad_count_.bind_base_as_ssbo(3);
    chunk_meta_.bind_base_as_ssbo(4);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_args.id());

    prog_mesh_count_.use();
    glUniform1ui(glGetUniformLocation(prog_mesh_count_.id, "u_chunk_hash_table_size"), chunk_hash_table_size);
    glUniform3i(glGetUniformLocation(prog_mesh_count_.id, "u_chunk_dim"), chunk_size.x, chunk_size.y, chunk_size.z);
    glUniform1ui(glGetUniformLocation(prog_mesh_count_.id, "u_voxels_per_chunk"), vox_per_chunk);
    glUniform1ui(glGetUniformLocation(prog_mesh_count_.id, "u_pack_bits"), pack_bits);
    glUniform1i(glGetUniformLocation(prog_mesh_count_.id, "u_pack_offset"), pack_offset);

    glDispatchComputeIndirect(0);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::mesh_alloc_vb(const BufferObject& dispatch_args) {
    mesh_buffers_status_.bind_base_as_ssbo(0);
    dirty_list_.bind_base_as_ssbo(1);
    dirty_quad_count_.bind_base_as_ssbo(2);
    chunk_meta_.bind_base_as_ssbo(3);
    chunk_mesh_alloc_local_.bind_base_as_ssbo(4);
    chunk_mesh_alloc_.bind_base_as_ssbo(5);

    vb_heads_.bind_base_as_ssbo(6);
    vb_state_.bind_base_as_ssbo(7);
    vb_nodes_.bind_base_as_ssbo(8);
    vb_free_nodes_list_.bind_base_as_ssbo(9);
    vb_returned_nodes_list.bind_base_as_ssbo(10);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_args.id());

    prog_mesh_alloc_.use();
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "bb_pages"), count_vb_pages_);
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "bb_page_elements"), vb_page_size_);
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "bb_max_order"), vb_order_);
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "bb_quad_size"), 4u);
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "u_is_vb_phase"), 1u);

    glDispatchComputeIndirect(0);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::mesh_alloc_ib(const BufferObject& dispatch_args) {
    mesh_buffers_status_.bind_base_as_ssbo(0);
    dirty_list_.bind_base_as_ssbo(1);
    dirty_quad_count_.bind_base_as_ssbo(2);
    chunk_meta_.bind_base_as_ssbo(3);
    chunk_mesh_alloc_local_.bind_base_as_ssbo(4);
    chunk_mesh_alloc_.bind_base_as_ssbo(5);

    ib_heads_.bind_base_as_ssbo(6);
    ib_state_.bind_base_as_ssbo(7);
    ib_nodes_.bind_base_as_ssbo(8);
    ib_free_nodes_list_.bind_base_as_ssbo(9);
    ib_returned_nodes_list.bind_base_as_ssbo(10);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_args.id());

    prog_mesh_alloc_.use();
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "bb_pages"), count_ib_pages_);
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "bb_page_elements"), ib_page_size_);
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "bb_max_order"), ib_order_);
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "bb_quad_size"), 6u);
    glUniform1ui(glGetUniformLocation(prog_mesh_alloc_.id, "u_is_vb_phase"), 0u);

    glDispatchComputeIndirect(0);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::mesh_alloc(const BufferObject& dispatch_args) {
    mesh_alloc_vb(dispatch_args);
    mesh_alloc_ib(dispatch_args);
}

void VoxelGridGPU::verify_mesh_allocation(const BufferObject& dispatch_args) {
    chunk_mesh_alloc_local_.bind_base_as_ssbo(0);
    chunk_mesh_alloc_.bind_base_as_ssbo(1);
    dirty_list_.bind_base_as_ssbo(2);
    mesh_buffers_status_.bind_base_as_ssbo(3);
    
    vb_heads_.bind_base_as_ssbo(4);
    vb_state_.bind_base_as_ssbo(5);
    vb_nodes_.bind_base_as_ssbo(6);
    vb_free_nodes_list_.bind_base_as_ssbo(7);
    vb_returned_nodes_list.bind_base_as_ssbo(8);
    
    ib_heads_.bind_base_as_ssbo(9);
    ib_state_.bind_base_as_ssbo(10);
    ib_nodes_.bind_base_as_ssbo(11);
    ib_free_nodes_list_.bind_base_as_ssbo(12);
    ib_returned_nodes_list.bind_base_as_ssbo(13);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_args.id());

    prog_verify_mesh_allocation_.use();
    glUniform1ui(glGetUniformLocation(prog_verify_mesh_allocation_.id, "vb_max_order"),  vb_order_);
    glUniform1ui(glGetUniformLocation(prog_verify_mesh_allocation_.id, "ib_max_order"),  ib_order_);

    glDispatchComputeIndirect(0);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::prepare_return_free_alloc_nodes(BufferObject& dispatch_args) {
    vb_returned_nodes_list.bind_base_as_ssbo(0);
    ib_returned_nodes_list.bind_base_as_ssbo(1);
    dispatch_args.bind_base_as_ssbo(2);

    prog_return_free_alloc_nodes_dispatch_adapter_.use();
    prog_return_free_alloc_nodes_dispatch_adapter_.dispatch_compute(1, 1, 1);
    glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::return_free_alloc_nodes(const BufferObject& dispatch_args) {
    vb_free_nodes_list_.bind_base_as_ssbo(0);
    vb_returned_nodes_list.bind_base_as_ssbo(1);

    ib_free_nodes_list_.bind_base_as_ssbo(2);
    ib_returned_nodes_list.bind_base_as_ssbo(3);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_args.id());

    prog_return_free_alloc_nodes_.use();
    glUniform3ui(glGetUniformLocation(prog_return_free_alloc_nodes_.id, "u3_chunk_size"), chunk_size.x, chunk_size.y, chunk_size.z);

    glDispatchComputeIndirect(0);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::mesh_emit(const BufferObject& dispatch_args, uint32_t pack_bits, uint32_t pack_offset) {
    chunk_hash_table_.bind_base_as_ssbo(0);
    voxels_.bind_base_as_ssbo(1);
    mesh_buffers_status_.bind_base_as_ssbo(2);
    dirty_list_.bind_base_as_ssbo(3);
    emit_counters_.bind_base_as_ssbo(4);
    chunk_mesh_alloc_.bind_base_as_ssbo(5);

    chunk_meta_.bind_base_as_ssbo(6);

    global_vertex_buffer_.bind_base_as_ssbo(7);
    global_index_buffer_.bind_base_as_ssbo(8);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_args.id());

    prog_mesh_emit_.use();
    glUniform1ui(glGetUniformLocation(prog_mesh_emit_.id, "u_chunk_hash_table_size"), chunk_hash_table_size);
    glUniform3i(glGetUniformLocation(prog_mesh_emit_.id, "u_chunk_dim"), chunk_size.x, chunk_size.y, chunk_size.z);
    glUniform1ui(glGetUniformLocation(prog_mesh_emit_.id, "u_voxels_per_chunk"), vox_per_chunk);
    glUniform3f(glGetUniformLocation(prog_mesh_emit_.id, "u_voxel_size"), voxel_size.x, voxel_size.y, voxel_size.z);

    glUniform1ui(glGetUniformLocation(prog_mesh_emit_.id, "u_pack_bits"), pack_bits);
    glUniform1i(glGetUniformLocation(prog_mesh_emit_.id, "u_pack_offset"), pack_offset);

    glUniform1ui(glGetUniformLocation(prog_mesh_emit_.id, "u_vb_page_verts"), vb_page_size_);
    glUniform1ui(glGetUniformLocation(prog_mesh_emit_.id, "u_ib_page_inds"), ib_page_size_);

    glDispatchComputeIndirect(0);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::mesh_finalize(const BufferObject& dispatch_args) {
    dirty_list_.bind_base_as_ssbo(0);
    enqueued_.bind_base_as_ssbo(1);
    chunk_meta_.bind_base_as_ssbo(2);
    chunk_mesh_alloc_.bind_base_as_ssbo(3);
    failed_dirty_list_.bind_base_as_ssbo(4);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_args.id());

    prog_mesh_finalize_.use();

    glDispatchComputeIndirect(0);
    
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::reset_dirty_count() {
    dirty_list_.bind_base_as_ssbo(0);

    vb_free_nodes_list_.bind_base_as_ssbo(1);
    vb_returned_nodes_list.bind_base_as_ssbo(2);

    ib_free_nodes_list_.bind_base_as_ssbo(3);
    ib_returned_nodes_list.bind_base_as_ssbo(4);

    prog_reset_dirty_count_.use();
    prog_reset_dirty_count_.dispatch_compute(1, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::build_mesh_from_dirty(uint32_t pack_bits, int pack_offset) {
    shader_helper->prepare_dispatch_args(dispatch_args, BufferDispatchArg(&dirty_list_, 0u));
    mesh_reset(dispatch_args);

    shader_helper->prepare_dispatch_args(dispatch_args, ValueDispatchArg(vox_per_chunk), BufferDispatchArg(&dirty_list_, 0u));
    mesh_count(dispatch_args, pack_bits, pack_offset);

    shader_helper->prepare_dispatch_args(dispatch_args, BufferDispatchArg(&dirty_list_, 0u));
    mesh_alloc(dispatch_args);

    shader_helper->prepare_dispatch_args(dispatch_args, BufferDispatchArg(&dirty_list_, 0u));
    verify_mesh_allocation(dispatch_args);

    prepare_return_free_alloc_nodes(dispatch_args);
    return_free_alloc_nodes(dispatch_args);

    shader_helper->prepare_dispatch_args(dispatch_args, ValueDispatchArg(vox_per_chunk), BufferDispatchArg(&dirty_list_, 0u));
    mesh_emit(dispatch_args, pack_bits, pack_offset);

    shader_helper->prepare_dispatch_args(dispatch_args, BufferDispatchArg(&dirty_list_, 0u));
    mesh_finalize(dispatch_args);

    reset_dirty_count();
}

void VoxelGridGPU::reset_cmd_count() {
    indirect_cmds_.update_subdata_fill<uint32_t>(0u, 0u, sizeof(uint32_t), *shader_manager);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::build_draw_commands(const glm::mat4& view_proj, const glm::vec3& cam_pos, uint32_t pack_bits, int pack_offset) {
    auto planes = math_utils::extract_frustum_planes(view_proj);

    chunk_meta_.bind_base_as_ssbo(0);
    chunk_mesh_alloc_.bind_base_as_ssbo(1);
    indirect_cmds_.bind_base_as_ssbo(2);

    prog_build_indirect_cmds_.use();

    glUniform1ui(glGetUniformLocation(prog_build_indirect_cmds_.id, "u_max_chunks"), count_active_chunks);
    glUniform3i(glGetUniformLocation(prog_build_indirect_cmds_.id, "u_chunk_dim"), chunk_size.x, chunk_size.y, chunk_size.z);
    glUniform3f(glGetUniformLocation(prog_build_indirect_cmds_.id, "u_voxel_size"), voxel_size.x, voxel_size.y, voxel_size.z);

    glUniform1ui(glGetUniformLocation(prog_build_indirect_cmds_.id, "u_pack_bits"), pack_bits);
    glUniform1i(glGetUniformLocation(prog_build_indirect_cmds_.id, "u_pack_offset"), pack_offset);

    glUniform1ui(glGetUniformLocation(prog_build_indirect_cmds_.id, "u_vb_page_verts"), vb_page_size_);
    glUniform1ui(glGetUniformLocation(prog_build_indirect_cmds_.id, "u_ib_page_inds"), ib_page_size_);

    glUniform3f(glGetUniformLocation(prog_build_indirect_cmds_.id, "cam_pos"), cam_pos.x, cam_pos.y, cam_pos.z);
    glUniform1f(glGetUniformLocation(prog_build_indirect_cmds_.id, "render_distance"), render_distance);

    GLint loc = glGetUniformLocation(prog_build_indirect_cmds_.id, "u_frustum_planes");
    glUniform4fv(loc, 6, &planes[0].x);

    uint32_t chunk_groups = math_utils::div_up_u32(count_active_chunks, 256u);
    prog_build_indirect_cmds_.dispatch_compute(chunk_groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelGridGPU::build_indirect_draw_commands_frustum(const glm::mat4& viewProj,
                                                        const glm::vec3& cam_pos,
                                                        uint32_t pack_bits,
                                                        int pack_offset) {
    reset_cmd_count();
    build_draw_commands(viewProj, cam_pos, pack_bits, pack_offset);
}

void VoxelGridGPU::draw_indirect(const GLuint vao, const glm::mat4& world, const glm::mat4& proj_view, const glm::vec3& cam_pos) {
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT |
            GL_ELEMENT_ARRAY_BARRIER_BIT |
            GL_COMMAND_BARRIER_BIT);

    glBindVertexArray(vao);

    // command buffer
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirect_cmds_.id());

    // индексы
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, global_index_buffer_.id());

    const GLsizei stride = (GLsizei)sizeof(DrawElementsIndirectCommand);
    const GLsizei maxDraws = (GLsizei)count_active_chunks;

    auto attash_shader_program = [&](){
        prog_vf_voxel_mesh_diffusion_spec_.use();
        glUniformMatrix4fv(glGetUniformLocation(prog_vf_voxel_mesh_diffusion_spec_.id,"uWorld"), 1, GL_FALSE, &world[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(prog_vf_voxel_mesh_diffusion_spec_.id,"uProjView"), 1, GL_FALSE, &proj_view[0][0]);
        glUniform3f(glGetUniformLocation(prog_vf_voxel_mesh_diffusion_spec_.id,"uViewPos"), cam_pos.x, cam_pos.y, cam_pos.z);
    };

    const GLintptr indirectOffset = sizeof(uint32_t);

    // Если доступно ARB_indirect_parameters -> НУЛЕВОЙ readback
    if (GLEW_ARB_indirect_parameters) {
        // drawcount берём прямо из frame_counters_.z (offset 8 байт: x,y,z,w)
        glBindBuffer(GL_PARAMETER_BUFFER, indirect_cmds_.id());

        const GLintptr countOffset    = 0;

        attash_shader_program();

        glMultiDrawElementsIndirectCountARB(
            GL_TRIANGLES,
            GL_UNSIGNED_INT,
            reinterpret_cast<const void*>(indirectOffset),
            countOffset,
            maxDraws,
            stride
        );
    }
    else {
        // Fallback: читаем только 4 байта cmdCount
        uint32_t cmdCount = indirect_cmds_.read_scalar<uint32_t>(0);

        attash_shader_program();

        glMultiDrawElementsIndirect(
            GL_TRIANGLES,
            GL_UNSIGNED_INT,
            reinterpret_cast<const void*>(indirectOffset),
            (GLsizei)cmdCount,
            stride
        );
    }

    glBindVertexArray(0);
}

void VoxelGridGPU::init_draw_buffers() {
    static VertexLayout vertex_layout;
    if (vertex_layout.attributes.size() == 0) {
        vertex_layout.add(
            "position",
            0, 4, GL_FLOAT, GL_FALSE,
            sizeof(VertexGPU),
            offsetof(VertexGPU, pos), 
            0, {0.0f, 0.0f, 0.0f, 1.0f}
        );
        vertex_layout.add(
            "color",
            1, 1, GL_UNSIGNED_INT, GL_FALSE,
            sizeof(VertexGPU),
            offsetof(VertexGPU, color), 
            0, {0xffffffffu} // белый
        );
        vertex_layout.add(
            "face",
            2, 1, GL_UNSIGNED_INT, GL_FALSE,
            sizeof(VertexGPU),
            offsetof(VertexGPU, face), 
            0, {0u} // Направление 0 (хз куда это, вверх мб?)
        );
    }

    vao.setup(global_vertex_buffer_, global_index_buffer_, vertex_layout);
}

void VoxelGridGPU::mark_all_used_chunks_as_dirty() {
    chunk_meta_.bind_base_as_ssbo(0);
    enqueued_.bind_base_as_ssbo(1);
    dirty_list_.bind_base_as_ssbo(2);

    prog_mark_all_user_chunks_as_dirty_.use();

    glUniform1ui(glGetUniformLocation(prog_mark_all_user_chunks_as_dirty_.id, "u_max_chunks"), count_active_chunks);

    uint32_t groups_x = math_utils::div_up_u32(count_active_chunks, 256u);
    prog_mark_all_user_chunks_as_dirty_.dispatch_compute(groups_x, 1, 1);

    glMemoryBarrier(GL_COMMAND_BARRIER_BIT);
}