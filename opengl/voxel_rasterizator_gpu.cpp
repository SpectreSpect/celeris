#include "voxel_rasterizator_gpu.h"

VoxelRasterizatorGPU::VoxelRasterizatorGPU(const VoxelRasterizatorDesc& desc, IGridable* gridable, ShaderManager* shader_manager, ShaderHelper* shader_helper) 
    :   chunk_size(desc.chunk_size),
        voxel_size(desc.voxel_size),
        counter_hash_table_size(desc.counter_hash_table_size),
        count_voxel_writes(desc.count_voxel_writes),
        gridable(gridable),
        gridable_gpu(dynamic_cast<IGridableGPU*>(gridable)),
        shader_manager(shader_manager),
        shader_helper(shader_helper)
{
    dispatch_args = BufferObject(sizeof(uint32_t) * 3, GL_DYNAMIC_DRAW);
    counter_hash_table_ = BufferObject(sizeof(HashTableCounters) + sizeof(CounterHashTableSlot) * desc.counter_hash_table_size, GL_DYNAMIC_DRAW);
    active_chunk_keys_list_ = BufferObject(sizeof(uint64_t) * (desc.counter_hash_table_size + 1u), GL_DYNAMIC_DRAW);
    triangle_indices_list_ = BufferObject::from_fill(sizeof(uint32_t) * (desc.counter_hash_table_size + 1u), GL_DYNAMIC_DRAW, 0u, *shader_manager);
    voxel_writes_ = BufferObject(sizeof(uint32_t) * 4 + sizeof(VoxelWriteGPU) * count_voxel_writes, GL_DYNAMIC_DRAW);

    init_programs();
}

VoxelRasterizatorGPU::~VoxelRasterizatorGPU() {

}

void VoxelRasterizatorGPU::init_programs() {
    prog_reset_voxelize_pipeline_ = ComputeProgram(&shader_manager->reset_voxelize_pipeline_cs);
    prog_mark_and_count_active_chunks_ = ComputeProgram(&shader_manager->mark_and_count_active_chunks_cs);
    prog_alloc_active_chunk_triangles_ = ComputeProgram(&shader_manager->alloc_active_chunk_triangles_cs);
    prog_fill_triangle_indices_ = ComputeProgram(&shader_manager->fill_triangle_indices_cs);
    prog_voxelize_triangles_ = ComputeProgram(&shader_manager->voxelize_triangles_cs);
}

void VoxelRasterizatorGPU::reset_voxelize_pipline(BufferObject& voxel_writes, bool reset_voxel_write_list) {
    counter_hash_table_.bind_base_as_ssbo(0);
    active_chunk_keys_list_.bind_base_as_ssbo(1);
    triangle_indices_list_.bind_base_as_ssbo(2);
    voxel_writes.bind_base_as_ssbo(3);

    prog_reset_voxelize_pipeline_.use();

    glUniform1ui(glGetUniformLocation(prog_reset_voxelize_pipeline_.id, "u_counter_hash_table_size"), counter_hash_table_size);
    glUniform1ui(glGetUniformLocation(prog_reset_voxelize_pipeline_.id, "u_reset_voxel_write_list"), reset_voxel_write_list ? 1u : 0u);

    uint32_t slot_groups = math_utils::div_up_u32(counter_hash_table_size, 256u);
    prog_reset_voxelize_pipeline_.dispatch_compute(slot_groups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelRasterizatorGPU::mark_and_count_active_chunks(const Mesh& mesh) {
    glm::mat4x4 transform = mesh.get_model_matrix();
    uint32_t count_triangles = mesh.ebo.size_bytes() / (sizeof(uint32_t) * 3);

    size_t position_attribute_id = mesh.vertex_layout.find_attribute_id_by_name("position");
    uint32_t vertex_stride_bytes = mesh.vertex_layout.attributes[position_attribute_id].stride;
    uint32_t position_offset_bytes = mesh.vertex_layout.attributes[position_attribute_id].offset;

    counter_hash_table_.bind_base_as_ssbo(0);
    active_chunk_keys_list_.bind_base_as_ssbo(1);
    mesh.vbo.bind_base_as_ssbo(2);
    mesh.ebo.bind_base_as_ssbo(3);

    prog_mark_and_count_active_chunks_.use();
    glUniform1ui(glGetUniformLocation(prog_mark_and_count_active_chunks_.id, "u_count_mesh_triangles"), count_triangles);

    glUniform1ui(glGetUniformLocation(prog_mark_and_count_active_chunks_.id, "u_counter_hash_table_size"), counter_hash_table_size);

    glUniform1ui(glGetUniformLocation(prog_mark_and_count_active_chunks_.id, "u_vertex_stride_bytes"), vertex_stride_bytes);
    glUniform1ui(glGetUniformLocation(prog_mark_and_count_active_chunks_.id, "u_vertex_position_offset_bytes"), position_offset_bytes);

    glUniform3f(glGetUniformLocation(prog_mark_and_count_active_chunks_.id, "u_voxel_size"), voxel_size.x, voxel_size.y, voxel_size.z);
    glUniform3ui(glGetUniformLocation(prog_mark_and_count_active_chunks_.id, "u_chunk_size"), chunk_size.x, chunk_size.y, chunk_size.z);

    glUniform1ui(glGetUniformLocation(prog_mark_and_count_active_chunks_.id, "u_pack_offset"), math_utils::OFFSET);
    glUniform1ui(glGetUniformLocation(prog_mark_and_count_active_chunks_.id, "u_pack_bits"), math_utils::BITS);

    glUniformMatrix4fv(glGetUniformLocation(prog_mark_and_count_active_chunks_.id, "u_transform"), 1, GL_FALSE, &transform[0][0]);

    uint32_t count_triangle_groups = math_utils::div_up_u32(count_triangles, 256u);
    prog_mark_and_count_active_chunks_.dispatch_compute(count_triangle_groups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelRasterizatorGPU::alloc_active_chunk_triangles(const BufferObject& dispatch_args) {    
    counter_hash_table_.bind_base_as_ssbo(0);
    active_chunk_keys_list_.bind_base_as_ssbo(1);
    triangle_indices_list_.bind_base_as_ssbo(2);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_args.id());

    prog_alloc_active_chunk_triangles_.use();

    glUniform1ui(glGetUniformLocation(prog_alloc_active_chunk_triangles_.id, "u_counter_hash_table_size"), counter_hash_table_size);

    glDispatchComputeIndirect(0);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelRasterizatorGPU::fill_triangle_indices(const Mesh& mesh) {
    glm::mat4x4 transform = mesh.get_model_matrix();
    uint32_t count_triangles = mesh.ebo.size_bytes() / (sizeof(uint32_t) * 3);

    counter_hash_table_.bind_base_as_ssbo(0);
    triangle_indices_list_.bind_base_as_ssbo(1);
    mesh.vbo.bind_base_as_ssbo(2);
    mesh.ebo.bind_base_as_ssbo(3);

    prog_fill_triangle_indices_.use();

    size_t position_attribute_id = mesh.vertex_layout.find_attribute_id_by_name("position");
    uint32_t vertex_stride_bytes = mesh.vertex_layout.attributes[position_attribute_id].stride;
    uint32_t position_offset_bytes = mesh.vertex_layout.attributes[position_attribute_id].offset;

    glUniform1ui(glGetUniformLocation(prog_fill_triangle_indices_.id, "u_count_mesh_triangles"), count_triangles);
    glUniform1ui(glGetUniformLocation(prog_fill_triangle_indices_.id, "u_counter_hash_table_size"), counter_hash_table_size);

    glUniform1ui(glGetUniformLocation(prog_fill_triangle_indices_.id, "u_vertex_stride_bytes"), vertex_stride_bytes);
    glUniform1ui(glGetUniformLocation(prog_fill_triangle_indices_.id, "u_vertex_position_offset_bytes"), position_offset_bytes);

    glUniform3f(glGetUniformLocation(prog_fill_triangle_indices_.id, "u_voxel_size"), voxel_size.x, voxel_size.y, voxel_size.z);
    glUniform3ui(glGetUniformLocation(prog_fill_triangle_indices_.id, "u_chunk_size"), chunk_size.x, chunk_size.y, chunk_size.z);

    glUniform1ui(glGetUniformLocation(prog_fill_triangle_indices_.id, "u_pack_offset"), math_utils::OFFSET);
    glUniform1ui(glGetUniformLocation(prog_fill_triangle_indices_.id, "u_pack_bits"), math_utils::BITS);
    
    glUniformMatrix4fv(glGetUniformLocation(prog_fill_triangle_indices_.id, "u_transform"), 1, GL_FALSE, &transform[0][0]);

    uint32_t count_triangle_groups = math_utils::div_up_u32(count_triangles, 256u);
    prog_fill_triangle_indices_.dispatch_compute(count_triangle_groups, 1, 1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelRasterizatorGPU::voxelize_chunks(
    const BufferObject& dispatch_args,
    const Mesh& mesh,
    BufferObject& voxel_writes,
    uint32_t voxel_type_flags,
    uint32_t voxel_color,
    uint32_t voxel_set_flags) 
{
    glm::mat4x4 transform = mesh.get_model_matrix();
    uint32_t count_triangles = mesh.ebo.size_bytes() / (sizeof(uint32_t) * 3);

    size_t position_attribute_id = mesh.vertex_layout.find_attribute_id_by_name("position");
    uint32_t vertex_stride_bytes = mesh.vertex_layout.attributes[position_attribute_id].stride;
    uint32_t position_offset_bytes = mesh.vertex_layout.attributes[position_attribute_id].offset;

    counter_hash_table_.bind_base_as_ssbo(0);
    active_chunk_keys_list_.bind_base_as_ssbo(1);
    triangle_indices_list_.bind_base_as_ssbo(2);
    mesh.vbo.bind_base_as_ssbo(3);
    mesh.ebo.bind_base_as_ssbo(4);
    voxel_writes.bind_base_as_ssbo(5);

    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, dispatch_args.id());

    prog_voxelize_triangles_.use();

    glUniform1ui(glGetUniformLocation(prog_voxelize_triangles_.id, "u_counter_hash_table_size"), counter_hash_table_size);
    glUniform1ui(glGetUniformLocation(prog_voxelize_triangles_.id, "u_count_voxels_in_chunk"), chunk_size.x * chunk_size.y * chunk_size.z);

    glUniform1ui(glGetUniformLocation(prog_voxelize_triangles_.id, "u_vertex_stride_bytes"), vertex_stride_bytes);
    glUniform1ui(glGetUniformLocation(prog_voxelize_triangles_.id, "u_vertex_position_offset_bytes"), position_offset_bytes);

    glUniform3ui(glGetUniformLocation(prog_voxelize_triangles_.id, "u_chunk_dim"), chunk_size.x, chunk_size.y, chunk_size.z);
    glUniform3f(glGetUniformLocation(prog_voxelize_triangles_.id, "u_voxel_size"), voxel_size.x, voxel_size.y, voxel_size.z);

    glUniformMatrix4fv(glGetUniformLocation(prog_voxelize_triangles_.id, "u_transform"), 1, GL_FALSE, &transform[0][0]);

    glUniform1ui(glGetUniformLocation(prog_voxelize_triangles_.id, "u_pack_offset"), math_utils::OFFSET);
    glUniform1ui(glGetUniformLocation(prog_voxelize_triangles_.id, "u_pack_bits"), math_utils::BITS);

    glUniform1ui(glGetUniformLocation(prog_voxelize_triangles_.id, "voxel_type_flags"), voxel_type_flags);
    glUniform1ui(glGetUniformLocation(prog_voxelize_triangles_.id, "voxel_color"), voxel_color);
    glUniform1ui(glGetUniformLocation(prog_voxelize_triangles_.id, "voxel_set_flags"), voxel_set_flags);

    glDispatchComputeIndirect(0);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelRasterizatorGPU::print_triangle_emmit_diff() {
    std::vector<CounterHashTableSlot> slot(counter_hash_table_size);
    counter_hash_table_.read_subdata(sizeof(HashTableCounters), sizeof(CounterHashTableSlot) * counter_hash_table_size, slot.data());

    uint32_t count_alloc = active_chunk_keys_list_.read_scalar<uint32_t>(0);

    uint32_t count_occupied = 0u;
    for (uint32_t slot_id = 0u; slot_id < counter_hash_table_size; slot_id++) {
        if (slot[slot_id].state == SLOT_OCCUPIED) {
            count_occupied++;

            uint32_t diff = slot[slot_id].value.triangle_emmit_counter - slot[slot_id].value.triangle_indices_base;
            std::cout << "SLOT_ID " << slot_id << ": " << "count_triangles = " << slot[slot_id].value.count_triangles << "   diff = " << diff << std::endl;
        }
    }
    std::cout << std::endl;

    std::cout << "COUNT_ALLOC: " << count_alloc << std::endl;
    std::cout << "COUNT_OCCUPIED: " << count_occupied << std::endl;
    std::cout << std::endl;
}

void VoxelRasterizatorGPU::print_hash_table_counters_state() {
    HashTableCounters counters = counter_hash_table_.read_scalar<HashTableCounters>(0);
    std::cout << "cout_empty: " << counters.reduce_read_count_empty() << std::endl;
    std::cout << "count_occupied: " << counters.reduce_read_count_occupied() << std::endl;
    std::cout << "count_tomb: " << counters.reduce_read_count_tomb() << std::endl;
    std::cout << std::endl;
}

void VoxelRasterizatorGPU::print_count_nonzero_triangles(const Mesh& mesh) {
    uint32_t count_triangles = triangle_indices_list_.read_scalar<uint32_t>(0);
    std::vector<uint32_t> triangles_list(count_triangles);
    triangle_indices_list_.read_subdata(sizeof(uint32_t), sizeof(uint32_t) * count_triangles, triangles_list.data());

    uint32_t count_nonzero_triangles = 0u;
    for (uint32_t i = 0u; i < count_triangles; i++) {
        if (triangles_list[i] != 0u) count_nonzero_triangles++;
    }

    std::cout << "REAL_COUNT_TRIANGLES: " << mesh.ebo.size_bytes() / (sizeof(uint32_t) * 3) << std::endl;
    std::cout << "COUNT_TRIANGLES: " << count_triangles << std::endl;
    std::cout << "COUNT_NONZERO_TRIANGLES: " << count_nonzero_triangles << std::endl;
}

void VoxelRasterizatorGPU::print_active_chunks() {
    uint32_t count_active_chunks = active_chunk_keys_list_.read_scalar<uint32_t>(0);
    std::vector<uint64_t> active_chunk_keys(count_active_chunks);
    active_chunk_keys_list_.read_subdata(sizeof(uint64_t), sizeof(uint64_t) * count_active_chunks, active_chunk_keys.data());

    for (uint32_t i = 0u; i < count_active_chunks; i++) {
        glm::ivec3 chunk_pos = math_utils::unpack_key(active_chunk_keys[i]);
        std::cout << "ID " << i << ":   " << chunk_pos.x << ", " << chunk_pos.y << ", " << chunk_pos.z << std::endl; 
    }
    std::cout << std::endl;
}

void VoxelRasterizatorGPU::print_total_count_triangles_in_hash_table() {
    std::vector<CounterHashTableSlot> slots(counter_hash_table_size);
    counter_hash_table_.read_subdata(sizeof(HashTableCounters), sizeof(CounterHashTableSlot) * counter_hash_table_size, slots.data());

    uint32_t total_count_triangles = 0u;
    for (uint32_t slot_id = 0u; slot_id < counter_hash_table_size; slot_id++) {
        if (slots[slot_id].state == SLOT_OCCUPIED) {
            total_count_triangles += slots[slot_id].value.count_triangles;
        }
    }
    
    std::cout << "TOTAL COUNT TRIANGLES: " << total_count_triangles << std::endl;
    std::cout << std::endl;
}

void VoxelRasterizatorGPU::print_triangle_list() {
    std::vector<uint32_t> triangle_list;
    uint32_t count_trinalges;

    triangle_indices_list_.read_subdata(0, sizeof(uint32_t), &count_trinalges);
    triangle_list.resize(count_trinalges);
    triangle_indices_list_.read_subdata(sizeof(uint32_t), sizeof(uint32_t) * count_trinalges, triangle_list.data());

    std::cout << "COUNT TRIANGLES: " << count_trinalges << std::endl;
    for (uint32_t triangle_id = 0u; triangle_id < count_trinalges; triangle_id++) {
        std::cout << "TRIANGLE_ID " << triangle_id << ": " << triangle_list[triangle_id] << std::endl;
    }
    std::cout << std::endl;
}

void VoxelRasterizatorGPU::rasterize(const Mesh& mesh, const VoxelWriteGPU& prifab, BufferObject* out_voxel_writes)
{
    BufferObject* voxel_writes_buffer = out_voxel_writes ? out_voxel_writes : &voxel_writes_;

    reset_voxelize_pipline(*voxel_writes_buffer, out_voxel_writes == nullptr);

    mark_and_count_active_chunks(mesh);

    shader_helper->prepare_dispatch_args(dispatch_args, BufferDispatchArg(&active_chunk_keys_list_, 0)); 
    alloc_active_chunk_triangles(dispatch_args);

    fill_triangle_indices(mesh);

    shader_helper->prepare_dispatch_args(
        dispatch_args, 
        ValueDispatchArg(chunk_size.x*chunk_size.y*chunk_size.z), 
        BufferDispatchArg(&active_chunk_keys_list_, 0)
    );
    voxelize_chunks(
        dispatch_args,
        mesh,
        *voxel_writes_buffer,
        prifab.voxel_data.type_flags,
        prifab.voxel_data.color,
        prifab.set_flags
    );

    if (out_voxel_writes == nullptr) {
        if (gridable_gpu != nullptr) {
            gridable_gpu->set_voxels(voxel_writes_);
        } else {
            //TODO
        }
    }
}
