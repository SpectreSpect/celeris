#include "voxel_grid_gpu_debugger.h"

VoxelGridGPUDebugger::VoxelGridGPUDebugger(VoxelGridGPU* voxel_grid_in, ShaderHelper* shader_helper_in, Window* window_in) 
: voxel_grid(voxel_grid_in), shader_helper(shader_helper_in), window(window_in) {

    std::fill(std::begin(voxel_grid_draw_streaming), std::end(voxel_grid_draw_streaming), voxel_grid_draw_streaming[0]);
    std::fill(std::begin(voxel_grid_generation_streaming), std::end(voxel_grid_generation_streaming), voxel_grid_generation_streaming[0]);
    
    voxel_grid_generation_streaming[6] = false;


    std::function<void()> build_mesh_from_dirty_fn = [&](){
        voxel_grid->build_mesh_from_dirty(math_utils::BITS, math_utils::OFFSET);
    };

    std::function<void()> build_indirect_draw_commands_frustum_fn = [&]() {
        float aspect = window->get_fbuffer_aspect_ratio();
        glm::mat4 view_matrix = window->camera->get_view_matrix();
        glm::mat4 proj_matrix = window->camera->get_projection_matrix(aspect);
        glm::mat4 view_proj_matrix = proj_matrix * view_matrix;
        voxel_grid->build_indirect_draw_commands_frustum(view_proj_matrix, window->camera->position, math_utils::BITS, math_utils::OFFSET);
    };

    std::function<void()> draw_indirect_fn = [&]() {
        float aspect = window->get_fbuffer_aspect_ratio();
        glm::mat4 view_matrix = window->camera->get_view_matrix();
        glm::mat4 proj_matrix = window->camera->get_projection_matrix(aspect);
        glm::mat4 view_proj_matrix = proj_matrix * view_matrix;

        voxel_grid->draw_indirect(voxel_grid->vao.id, glm::identity<glm::mat4>(), view_proj_matrix, window->camera->position);
    };

    voxel_grid_draw_steps = {build_mesh_from_dirty_fn, build_indirect_draw_commands_frustum_fn, draw_indirect_fn};

    


    std::function<void()> ensure_free_chunks_gpu_fn = [&](){
        voxel_grid->ensure_free_chunks_gpu(window->camera->position, math_utils::BITS, math_utils::OFFSET);
    };

    std::function<void()> reset_load_list_counter_fn = [&](){
        voxel_grid->reset_load_list_counter();
    };

    std::function<void()> mark_chunk_to_generate_fn = [&](){
        voxel_grid->mark_chunk_to_generate(window->camera->position, voxel_grid->generation_distance);
    };

    std::function<void()> merge_voxel_write_lists_fn = [&](){
        voxel_grid->merge_voxel_write_lists(voxel_grid->local_voxel_write_list_, voxel_grid->voxel_write_list_);
    };

    std::function<void()> reset_voxel_write_list_counter_local_fn = [&](){
        voxel_grid->reset_voxel_write_list_counter(voxel_grid->local_voxel_write_list_);
    };

    std::function<void()> mark_write_chunks_to_generate_fn = [&](){
        shader_helper->prepare_dispatch_args(voxel_grid->dispatch_args, BufferDispatchArg(&voxel_grid->voxel_write_list_, 0));
        voxel_grid->mark_write_chunks_to_generate(voxel_grid->dispatch_args);
    };

    std::function<void()> generate_terrain_fn = [&](){
        shader_helper->prepare_dispatch_args(voxel_grid->dispatch_args, ValueDispatchArg(voxel_grid->vox_per_chunk), BufferDispatchArg(&voxel_grid->load_list_, 0u));
        voxel_grid->generate_terrain(voxel_grid->dispatch_args, 45345345);
    };

    std::function<void()> write_voxels_to_grid_fn = [&](){
        shader_helper->prepare_dispatch_args(voxel_grid->dispatch_args, BufferDispatchArg(&voxel_grid->voxel_write_list_, 0u));
        voxel_grid->write_voxels_to_grid();
    };
    
    std::function<void()> reset_voxel_write_list_counter_fn = [&](){
        voxel_grid->reset_voxel_write_list_counter(voxel_grid->voxel_write_list_);
    };

    voxel_grid_generation_steps = {
        ensure_free_chunks_gpu_fn,
        reset_load_list_counter_fn,
        mark_chunk_to_generate_fn,
        merge_voxel_write_lists_fn,
        reset_voxel_write_list_counter_local_fn,
        mark_write_chunks_to_generate_fn,
        generate_terrain_fn,
        write_voxels_to_grid_fn,
        reset_voxel_write_list_counter_fn
    };
}

void VoxelGridGPUDebugger::print_finded_chunks_in_hash_table(glm::ivec3 chunk_pos) {
    std::vector<ChunkHashTableSlot> chunk_hash_table_slot(voxel_grid->chunk_hash_table_size);
    voxel_grid->chunk_hash_table_.read_subdata(sizeof(uint32_t) * 2, sizeof(ChunkHashTableSlot) * voxel_grid->chunk_hash_table_size, chunk_hash_table_slot.data());

    uint64_t key = math_utils::pack_key(chunk_pos.x, chunk_pos.y, chunk_pos.z);
    
    uint32_t count_matches = 0;
    std::cout << "==== HASH TABLE (" << chunk_pos.x << ", " << chunk_pos.y << ", " << chunk_pos.z << ") MATCHES ====" << std::endl;
    for (uint32_t slot_id = 0; slot_id < voxel_grid->chunk_hash_table_size; slot_id++) {
        if (key == chunk_hash_table_slot[slot_id].key) {
            std::string slot_value_str;
            if (chunk_hash_table_slot[slot_id].value == SLOT_EMPTY) slot_value_str = "SLOT_EMPTY";
            else if (chunk_hash_table_slot[slot_id].value == SLOT_TOMB) slot_value_str = "SLOT_TOMB";
            else if (chunk_hash_table_slot[slot_id].value == SLOT_LOCKED) slot_value_str = "SLOT_LOCKED";
            else slot_value_str = std::to_string(chunk_hash_table_slot[slot_id].value);

            std::cout << "SLOT_ID " << slot_id << ": " << "slot_value = " << slot_value_str << std::endl;
            count_matches++;
        }
    }

    if (count_matches == 0) {
        std::cout << "No matches has been detected." << std::endl;
    }

    std::cout << std::endl;
}


void VoxelGridGPUDebugger::print_counters() {
    uint32_t load_list_count = voxel_grid->load_list_.read_scalar<uint32_t>(0u);
    uint32_t write_count = voxel_grid->voxel_write_list_.read_scalar<uint32_t>(0);
    uint32_t dirty_count = voxel_grid->dirty_list_.read_scalar<uint32_t>(0);
    uint32_t cmd_count = voxel_grid->indirect_cmds_.read_scalar<uint32_t>(0);
    uint32_t free_count = voxel_grid->free_list_.read_scalar<uint32_t>(0);
    uint32_t failed_dirty_count = voxel_grid->failed_dirty_list_.read_scalar<uint32_t>(0);
    uint32_t is_vb_full = voxel_grid->mesh_buffers_status_.read_scalar<uint32_t>(0);
    uint32_t is_ib_full = voxel_grid->mesh_buffers_status_.read_scalar<uint32_t>(sizeof(uint32_t));

    std::cout << "write_count: " << write_count << std::endl;
    std::cout << "dirty_count: " << dirty_count << std::endl;
    std::cout << "cmd_count: " << cmd_count << std::endl;
    std::cout << "free_count: " << free_count << std::endl;
    std::cout << "failed_dirty_count: " << failed_dirty_count << std::endl;
    std::cout << "is_vb_full: " << (is_vb_full == 1u ? "TRUE" : "FALSE") << std::endl;
    std::cout << "count_ib_free_pages: " << (is_ib_full == 1u ? "TRUE" : "FALSE") << std::endl;
    std::cout << "load_list_count: " << load_list_count << std::endl;

    uint32_t count_free_nodes_vb = voxel_grid->vb_free_nodes_list_.read_scalar<uint32_t>(0);
    uint32_t count_free_nodes_ib = voxel_grid->ib_free_nodes_list_.read_scalar<uint32_t>(0);

    std::cout << "count_free_nodes_vb: " << count_free_nodes_vb << std::endl;
    std::cout << "count_free_nodes_ib: " << count_free_nodes_ib << std::endl;

    std::cout << std::endl;

    print_count_free_mesh_alloc();
}

void VoxelGridGPUDebugger::print_count_free_mesh_alloc() {
    std::vector<ChunkMeshAlloc> alloc_meta(voxel_grid->count_active_chunks);
    std::vector<uint32_t> vb_states(voxel_grid->count_vb_pages_);
    std::vector<uint32_t> ib_states(voxel_grid->count_ib_pages_);
    std::vector<uint32_t> vb_heads(voxel_grid->vb_order_ + 1);
    std::vector<uint32_t> ib_heads(voxel_grid->ib_order_ + 1);
    std::vector<AllocNode> vb_nodes(voxel_grid->count_vb_nodes_);
    std::vector<AllocNode> ib_nodes(voxel_grid->count_ib_nodes_);
    std::vector<uint32_t> dirty_list;
    uint32_t dirty_count;

    voxel_grid->chunk_mesh_alloc_.read_subdata(0, sizeof(ChunkMeshAlloc) * voxel_grid->count_active_chunks, alloc_meta.data());
    voxel_grid->vb_state_.read_subdata(0, sizeof(uint32_t) * voxel_grid->count_vb_pages_, vb_states.data());
    voxel_grid->ib_state_.read_subdata(0, sizeof(uint32_t) * voxel_grid->count_ib_pages_, ib_states.data());
    voxel_grid->vb_heads_.read_subdata(0, sizeof(uint32_t) * (voxel_grid->vb_order_ + 1), vb_heads.data());
    voxel_grid->ib_heads_.read_subdata(0, sizeof(uint32_t) * (voxel_grid->ib_order_ + 1), ib_heads.data());
    voxel_grid->vb_nodes_.read_subdata(0, sizeof(AllocNode) * voxel_grid->count_vb_nodes_, vb_nodes.data());
    voxel_grid->ib_nodes_.read_subdata(0, sizeof(AllocNode) * voxel_grid->count_ib_nodes_, ib_nodes.data());
    voxel_grid->dirty_list_.read_subdata(0, sizeof(uint32_t), &dirty_count);
    dirty_list.resize(dirty_count);
    voxel_grid->dirty_list_.read_subdata(sizeof(uint32_t), sizeof(uint32_t) * dirty_count, dirty_list.data());

    //=================РАСЧЁТ ДАННЫХ ПО MESH_ALLOC=================
    std::unordered_set<uint32_t> allocated_mesh;
    uint32_t count_alloc_vb_pages_from_meta = 0, count_alloc_ib_pages_from_meta = 0;
    uint32_t count_vb_alloc_chunks_from_meta = 0, count_ib_alloc_chunks_from_meta = 0;
    for (uint32_t i = 0; i < voxel_grid->count_active_chunks; i++) {
        ChunkMeshAlloc& meta = alloc_meta[i];
        if (meta.v_startPage != INVALID_ID) {
            count_alloc_vb_pages_from_meta += 1 << meta.v_order;
            count_vb_alloc_chunks_from_meta++;
            allocated_mesh.insert(i);
        }

        if (meta.i_startPage != INVALID_ID) {
            count_alloc_ib_pages_from_meta += 1 << meta.i_order;
            count_ib_alloc_chunks_from_meta++;
            allocated_mesh.insert(i);
        }
    }

    //=================РАСЧЁТ ПЕРЕСЕЧЕНИЙ ПО MESH_ALLOC=================
    std::vector<uint32_t> ids(allocated_mesh.begin(), allocated_mesh.end());
    std::vector<std::pair<uint32_t,uint32_t>> v_pairs, i_pairs;
    v_pairs.reserve(64); i_pairs.reserve(64);

    for (size_t a = 0; a < ids.size(); ++a) {
        for (size_t b = a + 1; b < ids.size(); ++b) {
            uint32_t ida = ids[a], idb = ids[b];
            const auto &A = alloc_meta[ida];
            const auto &B = alloc_meta[idb];

            uint64_t vsA = A.v_startPage, vsB = B.v_startPage;
            uint64_t vlA = (A.v_order < 64) ? (1ull << A.v_order) : UINT64_MAX;
            uint64_t vlB = (B.v_order < 64) ? (1ull << B.v_order) : UINT64_MAX;
            if (math_utils::intersects(vsA, vlA, vsB, vlB)) v_pairs.emplace_back(ida, idb);

            uint64_t isA = A.i_startPage, isB = B.i_startPage;
            uint64_t ilA = (A.i_order < 64) ? (1ull << A.i_order) : UINT64_MAX;
            uint64_t ilB = (B.i_order < 64) ? (1ull << B.i_order) : UINT64_MAX;
            if (math_utils::intersects(isA, ilA, isB, ilB)) i_pairs.emplace_back(ida, idb);
        }
    }
    
    //=================РАСЧЁТ ДАННЫХ VB ПО STATES=================
    uint32_t count_vb_free = 0, vb_free_pages = 0;
    uint32_t count_vb_alloc = 0, vb_alloc_pages = 0;
    uint32_t count_vb_merged = 0, vb_merged_pages = 0;
    uint32_t count_vb_merging = 0, vb_merging_pages = 0;
    uint32_t count_vb_ready = 0, vb_ready_pages = 0;
    uint32_t count_vb_conceded = 0, vb_conceded_pages = 0;
    for (uint32_t i = 0; i < voxel_grid->count_vb_pages_; i++) {
        uint32_t kind = vb_states[i] & ST_MASK;
        uint32_t order = vb_states[i] >> ST_MASK_BITS;
        uint32_t count_pages = 1u << order;
        if (kind == ST_FREE) {count_vb_free++; vb_free_pages += count_pages; }
        if (kind == ST_ALLOC) {count_vb_alloc++; vb_alloc_pages += count_pages; }
        if (kind == ST_MERGED) {count_vb_merged++; vb_merged_pages += count_pages; }
    }

    //=================РАСЧЁТ ДАННЫХ IB ПО STATES=================
    uint32_t count_ib_free = 0, ib_free_pages = 0;
    uint32_t count_ib_alloc = 0, ib_alloc_pages = 0;
    uint32_t count_ib_merged = 0, ib_merged_pages = 0;
    uint32_t count_ib_merging = 0, ib_merging_pages = 0;
    uint32_t count_ib_ready = 0, ib_ready_pages = 0;
    uint32_t count_ib_conceded = 0, ib_conceded_pages = 0;
    for (uint32_t i = 0; i < voxel_grid->count_ib_pages_; i++) {
        uint32_t kind = ib_states[i] & ST_MASK;
        uint32_t order = ib_states[i] >> ST_MASK_BITS;
        uint32_t count_pages = 1u << order;
        if (kind == ST_FREE) {count_ib_free++; ib_free_pages += count_pages; }
        if (kind == ST_ALLOC) {count_ib_alloc++; ib_alloc_pages += count_pages; }
        if (kind == ST_MERGED) {count_ib_merged++; ib_merged_pages += count_pages; }
    }

    //=================РАСЧЁТ ДАННЫХ VB ПО HEADS=================
    std::vector<uint32_t> count_free_states_by_vb_order(voxel_grid->vb_order_ + 1, 0);
    std::vector<uint32_t> count_free_pages_by_vb_order(voxel_grid->vb_order_ + 1, 0);
    uint32_t count_free_states_by_vb_heads = 0, count_free_pages_by_vb_heads = 0;
    for (uint32_t order = 0; order <= voxel_grid->vb_order_; order++) {
        uint32_t head_idx = vb_heads[order] >> HEAD_TAG_BITS;
        uint32_t cur_node = head_idx != INVALID_HEAD_IDX ? head_idx : INVALID_ID;
        uint32_t order_size = 1u << order;
        while (cur_node != INVALID_ID) {
            uint32_t page_id = vb_nodes[cur_node].page;
            uint32_t kind = vb_states[page_id] & ST_MASK;
            uint32_t real_order = vb_states[page_id] >> ST_MASK_BITS;
            if (kind == ST_FREE && real_order == order) {
                count_free_states_by_vb_order[order]++;
                count_free_pages_by_vb_order[order] += order_size;

                count_free_states_by_vb_heads++;
                count_free_pages_by_vb_heads += order_size;
            }
            cur_node = vb_nodes[cur_node].next;
        }
    }

    //=================РАСЧЁТ ДАННЫХ IB ПО HEADS=================
    std::vector<uint32_t> count_free_states_by_ib_order(voxel_grid->ib_order_ + 1, 0);
    std::vector<uint32_t> count_free_pages_by_ib_order(voxel_grid->ib_order_ + 1, 0);
    uint32_t count_free_states_by_ib_heads = 0, count_free_pages_by_ib_heads = 0;
    for (uint32_t order = 0; order <= voxel_grid->ib_order_; order++) {
        uint32_t head_idx = ib_heads[order] >> HEAD_TAG_BITS;
        uint32_t cur_node = head_idx != INVALID_HEAD_IDX ? head_idx : INVALID_ID;
        uint32_t order_size = 1u << order;
        while (cur_node != INVALID_ID) {
            uint32_t page_id = ib_nodes[cur_node].page;
            uint32_t kind = ib_states[page_id] & ST_MASK;
            uint32_t real_order = ib_states[page_id] >> ST_MASK_BITS;
            if (kind == ST_FREE && real_order == order) {
                count_free_states_by_ib_order[order]++;
                count_free_pages_by_ib_order[order] += order_size;

                count_free_states_by_ib_heads++;
                count_free_pages_by_ib_heads += order_size;
            }
            cur_node = ib_nodes[cur_node].next;
        }
    }


    std::cout << "---INTERSECTIONS---" << std::endl;
    std::cout << "==Vertex buffer==" << std::endl;
    std::cout << "Count vb intersections: " << v_pairs.size() << std::endl;
    std::cout << std::endl;
    std::cout << "==Index buffer==" << std::endl;
    std::cout << "Count ib intersections: " << i_pairs.size() << std::endl;
    std::cout << std::endl;

    std::cout << "---DATA FROM STATES BUFFER---" << std::endl;
    std::cout << "==Vertex buffer==" << std::endl;
    std::cout << "ST_FREE:      count states = " << count_vb_free     << "  count pages = " << vb_free_pages << std::endl;
    std::cout << "ST_ALLOC:     count states = " << count_vb_alloc    << "  count pages = " << vb_alloc_pages << std::endl;
    std::cout << "ST_MERGED:    count states = " << count_vb_merged   << "  count pages = " << vb_merged_pages << std::endl;
    std::cout << "ST_MERGING:   count states = " << count_vb_merging  << "  count pages = " << vb_merging_pages << std::endl;
    std::cout << "ST_READY:     count states = " << count_vb_ready    << "  count pages = " << vb_ready_pages << std::endl;
    std::cout << "ST_CONCEDED:  count states = " << count_vb_conceded << "  count pages = " << vb_conceded_pages << std::endl;
    std::cout << "SUM (free + alloc): count states = " << count_vb_free + count_vb_alloc << "  count pages = " << vb_free_pages + vb_alloc_pages << std::endl;
    std::cout << "REAL_COUNT_PAGES: " << voxel_grid->count_vb_pages_ << std::endl;
    std::cout << "LIMBO: " << (int)voxel_grid->count_vb_pages_ - (vb_free_pages + vb_alloc_pages) << std::endl;
    std::cout << std::endl;
    std::cout << "==Index buffer==" << std::endl;
    std::cout << "ST_FREE:      count states = " << count_ib_free     << "  count pages = " << ib_free_pages << std::endl;
    std::cout << "ST_ALLOC:     count states = " << count_ib_alloc    << "  count pages = " << ib_alloc_pages << std::endl;
    std::cout << "ST_MERGED:    count states = " << count_ib_merged   << "  count pages = " << ib_merged_pages << std::endl;
    std::cout << "ST_MERGING:   count states = " << count_ib_merging  << "  count pages = " << ib_merging_pages << std::endl;
    std::cout << "ST_READY:     count states = " << count_ib_ready    << "  count pages = " << ib_ready_pages << std::endl;
    std::cout << "ST_CONCEDED:  count states = " << count_ib_conceded << "  count pages = " << ib_conceded_pages << std::endl;
    std::cout << "SUM (free + alloc): count states = " << count_ib_free + count_ib_alloc << "  count pages = " << ib_free_pages + ib_alloc_pages << std::endl;
    std::cout << "REAL_COUNT_PAGES: " << voxel_grid->count_ib_pages_ << std::endl;
    std::cout << "LIMBO: " << (int)voxel_grid->count_ib_pages_ - (ib_free_pages + ib_alloc_pages) << std::endl;
    std::cout << std::endl;

    std::cout << "---DATA FROM META---"      << std::endl;
    std::cout << "==Vertex buffer==" << std::endl;
    std::cout << "ST_ALLOC:      count chunks = " << count_vb_alloc_chunks_from_meta << " count pages = " << count_alloc_vb_pages_from_meta << std::endl;
    std::cout << "COUNT_ALLOC_PAGES_BY_STATES: " << vb_alloc_pages << std::endl;
    std::cout << "LIMBO (by STATES): " << (int)vb_alloc_pages - count_alloc_vb_pages_from_meta << std::endl;
    std::cout << std::endl;

    std::cout << "==Index buffer==" << std::endl;
    std::cout << "ST_ALLOC:      count chunks = " << count_ib_alloc_chunks_from_meta << " count pages = " << count_alloc_ib_pages_from_meta << std::endl;
    std::cout << "COUNT_ALLOC_PAGES_BY_STATES: " << ib_alloc_pages << std::endl;
    std::cout << "LIMBO (by STATES): " << (int)ib_alloc_pages - count_alloc_ib_pages_from_meta << std::endl;
    std::cout << std::endl;

    std::cout << "---DATA FROM HEADS---" << std::endl;
    std::cout << "==Vertex buffer==" << std::endl;
    std::cout << "ST_FREE:      count states = " << count_free_states_by_vb_heads << "  count pages = " << count_free_pages_by_vb_heads << std::endl;
    std::cout << "COUNT_FREE_PAGES_BY_STATES: " << vb_free_pages << std::endl;
    std::cout << "LIMBO (by STATES): " << (int)vb_free_pages - count_free_pages_by_vb_heads << std::endl;
    std::cout << std::endl;
    std::cout << "VB data per order:" << std::endl;
    for (uint32_t order = 0; order <= voxel_grid->vb_order_; order++) {
        std::cout << std::left << std::setw(10) << ("ORDER " + std::to_string(order) + ":")
                  << std::right << std::setw(14 + 5) << "count states ="
                  << std::right << std::setw(7) << count_free_states_by_vb_order[order]
                  << std::right << std::setw(13 + 5) << "count pages ="
                  << std::right << std::setw(7) << count_free_pages_by_vb_order[order]
                  << std::endl;
    }
    std::cout << std::endl;

    std::cout << "==Index buffer==" << std::endl;
    std::cout << "ST_FREE:      count states = " << count_free_states_by_ib_heads << "  count pages = " << count_free_pages_by_ib_heads << std::endl;
    std::cout << "COUNT_FREE_PAGES_BY_STATES: " << ib_free_pages << std::endl;
    std::cout << "LIMBO (by STATES): " << (int)ib_free_pages - count_free_pages_by_ib_heads << std::endl;
    std::cout << std::endl;
    std::cout << "IB data per order:" << std::endl;
    for (uint32_t order = 0; order <= voxel_grid->ib_order_; order++) {
        std::cout << std::left << std::setw(10) << ("ORDER " + std::to_string(order) + ":")
                  << std::right << std::setw(14 + 5) << "count states ="
                  << std::right << std::setw(7) << count_free_states_by_ib_order[order]
                  << std::right << std::setw(13 + 5) << "count pages ="
                  << std::right << std::setw(7) << count_free_pages_by_ib_order[order]
                  << std::endl;
    }
    std::cout << std::endl;
}

void VoxelGridGPUDebugger::print_chunks_hash_table_log() {
    std::vector<ChunkHashTableSlot> chunk_hash_table_slot(voxel_grid->chunk_hash_table_size);
    voxel_grid->chunk_hash_table_.read_subdata(sizeof(HashTableCounters), sizeof(ChunkHashTableSlot) * voxel_grid->chunk_hash_table_size, chunk_hash_table_slot.data());

    HashTableCounters counters = voxel_grid->chunk_hash_table_.read_scalar<HashTableCounters>(0u);

    uint32_t count_empty_slots = 0u, count_lock_slots = 0u, count_tomb_slots = 0u, count_occupied_slots = 0u, count_error_states = 0u; 
    for (uint32_t slot_id = 0u; slot_id < voxel_grid->chunk_hash_table_size; slot_id++) {
        uint32_t state = chunk_hash_table_slot[slot_id].state;

        if (state == SLOT_EMPTY) count_empty_slots++;
        else if (state == SLOT_LOCKED) count_lock_slots++;
        else if (state == SLOT_TOMB) count_tomb_slots++;
        else if (state == SLOT_OCCUPIED) count_occupied_slots++;
        else count_error_states++;
    }

    auto get_count_string = [&](uint32_t count_cpu) -> std::string {
        std::ostringstream ss;

        float cpu_percent = static_cast<float>(count_cpu) / voxel_grid->chunk_hash_table_size * 100.0f;

        ss << count_cpu
        << " (" << std::fixed << std::setprecision(2) << cpu_percent << "%)";

        return ss.str();
    };

    std::cout << "======= CHUNKS HASH TABLE LOG =======" << std::endl;
    std::cout << "Total count hash table slots: " << voxel_grid->chunk_hash_table_size << std::endl;

    std::cout << "**CPU**:" << std::endl;
    std::cout << "SLOT_EMPTY: " << get_count_string(count_empty_slots) << std::endl;
    std::cout << "SLOT_TOMB: " << get_count_string(count_tomb_slots) << std::endl;
    std::cout << "SLOT_OCCUPIED: " << get_count_string(count_occupied_slots) << std::endl;
    std::cout << "SLOT_LOCKED: " << count_lock_slots << "(" 
              << std::fixed << std::setprecision(2) << (float)count_lock_slots / voxel_grid->chunk_hash_table_size * 100.0f << "%)" << std::endl;

    std::cout << "ERROR_SLOTS: " << count_error_states << "(" 
              << std::fixed << std::setprecision(2) << (float)count_error_states / voxel_grid->chunk_hash_table_size * 100.0f << "%)" << std::endl;
    std::cout << std::endl;

    std::cout << "**GPU**:" << std::endl;
    std::cout << "SLOT_EMPTY: " << get_count_string(counters.reduce_read_count_empty()) << std::endl;
    std::cout << "SLOT_TOMB: " << get_count_string(counters.reduce_read_count_tomb()) << std::endl;
    std::cout << "SLOT_OCCUPIED: " << get_count_string(counters.reduce_read_count_occupied()) << std::endl;

    std::cout << std::endl;
}

// void VoxelGridGPUDebugger::print_eviction_log(const glm::vec3& camera_pos) {
//     std::vector<BucketHead> bucket_heads(voxel_grid->count_evict_buckets);
//     std::vector<uint32_t> bucket_next(voxel_grid->count_active_chunks);
//     std::vector<ChunkMetaGPU> chunk_meta(voxel_grid->count_active_chunks);

//     voxel_grid->bucket_heads_.read_subdata(0, sizeof(BucketHead) * voxel_grid->count_evict_buckets, bucket_heads.data());
//     voxel_grid->bucket_next_.read_subdata(0, sizeof(uint32_t) * voxel_grid->count_active_chunks, bucket_next.data());
//     voxel_grid->chunk_meta_.read_subdata(0, sizeof(ChunkMetaGPU) * voxel_grid->count_active_chunks, chunk_meta.data());


//     struct ChunkInBucketData {
//         uint32_t chunk_id;
//         glm::ivec3 coords;
//         double distance_to_chunk;
//         uint32_t bucket_id_by_distance;
//     };
    
//     // ==================Подсчёт по HEADS==================
//     std::vector<uint32_t> count_chunks_per_bucket(voxel_grid->count_evict_buckets, 0u);
//     std::vector<uint32_t> count_chunk_mismatches_per_bucket(voxel_grid->count_evict_buckets, 0u);
//     uint32_t total_chunks_number_in_buckets = 0u, total_chunk_mismatches_in_buckets = 0u;
//     std::vector<std::vector<ChunkInBucketData>> chunks_per_bucket(voxel_grid->count_evict_buckets);
//     for (uint32_t bucket_id = 0; bucket_id < voxel_grid->count_evict_buckets; bucket_id++) {
//         uint32_t cur_id = bucket_heads[bucket_id];
        
//         while (cur_id != INVALID_ID) {
//             count_chunks_per_bucket[bucket_id]++;
//             total_chunks_number_in_buckets++;

//             ChunkInBucketData chunk_in_bucket;
//             chunk_in_bucket.chunk_id = cur_id;

//             uint64_t coords_key = ((uint64_t)(chunk_meta[cur_id].key_hi) << 32u) | (uint64_t)(chunk_meta[cur_id].key_lo);
//             chunk_in_bucket.coords = math_utils::unpack_key(coords_key);

//             glm::vec3 render_chunk_pos = glm::vec3(chunk_in_bucket.coords * voxel_grid->chunk_size) * voxel_grid->voxel_size;
//             glm::vec3 render_chunk_center = render_chunk_pos + glm::vec3(0.5) * glm::vec3(voxel_grid->chunk_size) * voxel_grid->voxel_size;
//             chunk_in_bucket.distance_to_chunk = glm::length(render_chunk_center - camera_pos);
//             chunk_in_bucket.bucket_id_by_distance = (uint32_t)(chunk_in_bucket.distance_to_chunk / voxel_grid->eviction_bucket_shell_thickness);

//             chunks_per_bucket[bucket_id].push_back(chunk_in_bucket);

//             if (bucket_id != chunk_in_bucket.bucket_id_by_distance) {
//                 count_chunk_mismatches_per_bucket[bucket_id]++;
//                 total_chunk_mismatches_in_buckets++;
//             }

//             cur_id = bucket_next[cur_id];
//         }
//     }

//     std::vector<double> min_distance_in_shell(voxel_grid->count_evict_buckets, std::numeric_limits<double>::max());
//     std::vector<double> max_distance_in_shell(voxel_grid->count_evict_buckets, 0.0);
//     for (uint32_t bucket_id = 0; bucket_id < voxel_grid->count_evict_buckets; bucket_id++) {
//         for (const ChunkInBucketData& chunk_data : chunks_per_bucket[bucket_id]) {
//             if (chunk_data.distance_to_chunk < min_distance_in_shell[bucket_id])
//                 min_distance_in_shell[bucket_id] = chunk_data.distance_to_chunk;
            
//             if (chunk_data.distance_to_chunk > max_distance_in_shell[bucket_id])
//                 max_distance_in_shell[bucket_id] = chunk_data.distance_to_chunk;
//         }
//     }
    

//     // ==================Вывод==================

//     std::cout << "========= DATA BY HEADS =========" << std::endl;
//     std::cout << "Total number of chunks in buckets: " << total_chunks_number_in_buckets << std::endl;
//     std::cout << "Total number of chunk mismatches in buckets: " << total_chunk_mismatches_in_buckets << std::endl;
//     std::cout << std::endl;
//     std::cout << "Data per heads:" << std::endl;
    
//     for (uint32_t bucket_id = 0u; bucket_id < voxel_grid->count_evict_buckets; bucket_id++) {
//         std::cout << std::left << std::setw(11 + 3) << ("BUCKET_ID " + std::to_string(bucket_id) + ":")
//                   << std::right << std::setw(14 + 5) << "count chunks ="
//                   << std::right << std::setw(5) << count_chunks_per_bucket[bucket_id]
//                   << std::right << std::setw(18 + 5) << "count mismatches ="
//                   << std::right << std::setw(5) << count_chunk_mismatches_per_bucket[bucket_id]
//                   << std::right << std::setw(15 + 5) << "min distance ="
//                   << std::right << std::setw(7) << min_distance_in_shell[bucket_id]
//                   << std::right << std::setw(14 + 5) << "max distance ="
//                   << std::right << std::setw(7) << max_distance_in_shell[bucket_id] << std::endl;
//     }
//     std::cout << std::endl;
// }

void VoxelGridGPUDebugger::print_eviction_log(const glm::vec3& camera_pos) {
    std::vector<BucketHead> bucket_heads(voxel_grid->count_evict_buckets);
    std::vector<uint32_t> bucket_next(voxel_grid->count_active_chunks);
    std::vector<ChunkMetaGPU> chunk_meta(voxel_grid->count_active_chunks);

    voxel_grid->bucket_heads_.read_subdata(0, sizeof(BucketHead) * voxel_grid->count_evict_buckets, bucket_heads.data());
    voxel_grid->bucket_next_.read_subdata(0, sizeof(uint32_t) * voxel_grid->count_active_chunks, bucket_next.data());
    voxel_grid->chunk_meta_.read_subdata(0, sizeof(ChunkMetaGPU) * voxel_grid->count_active_chunks, chunk_meta.data());

    struct ChunkInBucketData {
        uint32_t chunk_id;
        glm::ivec3 coords;
        double distance_to_chunk;
        uint32_t bucket_id_by_distance;
    };
    
    // ==================Подсчёт по HEADS==================
    std::vector<uint32_t> count_chunks_per_bucket(voxel_grid->count_evict_buckets, 0u);
    std::vector<uint32_t> count_chunk_mismatches_per_bucket(voxel_grid->count_evict_buckets, 0u);
    uint32_t total_chunks_number_in_buckets = 0u, total_chunk_mismatches_in_buckets = 0u;
    std::vector<std::vector<ChunkInBucketData>> chunks_per_bucket(voxel_grid->count_evict_buckets);
    for (uint32_t bucket_id = 0; bucket_id < voxel_grid->count_evict_buckets; bucket_id++) {
        uint32_t cur_id = bucket_heads[bucket_id].id;
        
        while (cur_id != INVALID_ID) {
            count_chunks_per_bucket[bucket_id]++;
            total_chunks_number_in_buckets++;

            ChunkInBucketData chunk_in_bucket;
            chunk_in_bucket.chunk_id = cur_id;

            uint64_t coords_key = ((uint64_t)(chunk_meta[cur_id].key_hi) << 32u) | (uint64_t)(chunk_meta[cur_id].key_lo);
            chunk_in_bucket.coords = math_utils::unpack_key(coords_key);

            glm::vec3 render_chunk_pos = glm::vec3(chunk_in_bucket.coords * voxel_grid->chunk_size) * voxel_grid->voxel_size;
            glm::vec3 render_chunk_center = render_chunk_pos + glm::vec3(0.5) * glm::vec3(voxel_grid->chunk_size) * voxel_grid->voxel_size;
            chunk_in_bucket.distance_to_chunk = glm::length(render_chunk_center - camera_pos);
            chunk_in_bucket.bucket_id_by_distance = (uint32_t)(chunk_in_bucket.distance_to_chunk / voxel_grid->eviction_bucket_shell_thickness);

            chunks_per_bucket[bucket_id].push_back(chunk_in_bucket);

            if (bucket_id != chunk_in_bucket.bucket_id_by_distance) {
                count_chunk_mismatches_per_bucket[bucket_id]++;
                total_chunk_mismatches_in_buckets++;
            }

            cur_id = bucket_next[cur_id];
        }
        
        std::cout << std::endl;
    }

    std::vector<double> min_distance_in_shell(voxel_grid->count_evict_buckets, std::numeric_limits<double>::max());
    std::vector<double> max_distance_in_shell(voxel_grid->count_evict_buckets, 0.0);
    for (uint32_t bucket_id = 0; bucket_id < voxel_grid->count_evict_buckets; bucket_id++) {
        for (const ChunkInBucketData& chunk_data : chunks_per_bucket[bucket_id]) {
            if (chunk_data.distance_to_chunk < min_distance_in_shell[bucket_id])
                min_distance_in_shell[bucket_id] = chunk_data.distance_to_chunk;
            
            if (chunk_data.distance_to_chunk > max_distance_in_shell[bucket_id])
                max_distance_in_shell[bucket_id] = chunk_data.distance_to_chunk;
        }
    }
    

    // ==================Вывод==================

    std::cout << "========= DATA BY HEADS =========" << std::endl;
    std::cout << "Total number of chunks in buckets: " << total_chunks_number_in_buckets << std::endl;
    std::cout << "Total number of chunk mismatches in buckets: " << total_chunk_mismatches_in_buckets << std::endl;
    std::cout << std::endl;
    std::cout << "Data per heads:" << std::endl;
    
    for (uint32_t bucket_id = 0u; bucket_id < voxel_grid->count_evict_buckets; bucket_id++) {
        std::cout << std::left << std::setw(11 + 3) << ("BUCKET_ID " + std::to_string(bucket_id) + ":")
                  << std::right << std::setw(14 + 5) << "count chunks ="
                  << std::right << std::setw(5) << count_chunks_per_bucket[bucket_id]
                  << std::right << std::setw(25 + 5) << "count chunks (from gpu) ="
                  << std::right << std::setw(5) << bucket_heads[bucket_id].count
                  << std::right << std::setw(18 + 5) << "count mismatches ="
                  << std::right << std::setw(5) << count_chunk_mismatches_per_bucket[bucket_id]
                  << std::right << std::setw(15 + 5) << "min distance ="
                  << std::right << std::setw(7) << min_distance_in_shell[bucket_id]
                  << std::right << std::setw(14 + 5) << "max distance ="
                  << std::right << std::setw(7) << max_distance_in_shell[bucket_id] << std::endl;
    }
    std::cout << std::endl;
}

void VoxelGridGPUDebugger::print_dirty_list() {
    uint32_t dirty_count = voxel_grid->dirty_list_.read_scalar<uint32_t>(0u);
    std::vector<uint32_t> dirty_list(dirty_count);
    voxel_grid->dirty_list_.read_subdata(sizeof(uint32_t), sizeof(uint32_t) * dirty_count, dirty_list.data());

    std::cout << "DIRTY_LIST: " << std::endl;
    for (uint32_t dirty_id = 0u; dirty_id < dirty_count && dirty_id < 100u; dirty_id++) {
        std::cout << "dirty_id " << dirty_id << ": " << dirty_list[dirty_id] << std::endl;
    }
    std::cout << std::endl;
}

void VoxelGridGPUDebugger::print_dirty_list_emit_counters() {
    uint32_t dirty_count = voxel_grid->dirty_list_.read_scalar<uint32_t>(0u);
    std::vector<uint32_t> dirty_list(dirty_count);
    std::vector<uint32_t> emit_counters(voxel_grid->count_active_chunks);
    
    voxel_grid->dirty_list_.read_subdata(sizeof(uint32_t), sizeof(uint32_t) * dirty_count, dirty_list.data());
    voxel_grid->emit_counters_.read_subdata(0, sizeof(uint32_t) * voxel_grid->count_active_chunks, emit_counters.data());

    std::cout << "EMIT COUNTERS: " << std::endl;
    for (uint32_t dirty_id = 0u; dirty_id < dirty_count && dirty_id < 100u; dirty_id++) {
        uint32_t chunk_id = dirty_list[dirty_id];
        std::cout << "dirty_id " << dirty_id << " chunk_id " << chunk_id << ": " << emit_counters[chunk_id] << std::endl;
    }
    std::cout << std::endl;
}

void VoxelGridGPUDebugger::print_dirty_list_quad_count() {
    uint32_t dirty_count = voxel_grid->dirty_list_.read_scalar<uint32_t>(0u);
    std::vector<uint32_t> dirty_quad_count(dirty_count);
    
    voxel_grid->dirty_quad_count_.read_subdata(0, sizeof(uint32_t) * dirty_count, dirty_quad_count.data());

    std::cout << "DIRTY QUAD COUNT: " << dirty_count << std::endl;
    std::cout << "DIRTY QUAD COUNTERS: " << std::endl;
    for (uint32_t dirty_id = 0u; dirty_id < dirty_count && dirty_id < 100u; dirty_id++) {
        std::cout << "dirty_id " << dirty_id << ": " << dirty_quad_count[dirty_id] << std::endl;
    }
    std::cout << std::endl;
}

void VoxelGridGPUDebugger::print_mesh_alloc_by_dirty_list(const std::string& prefix, uint32_t mesh_alloc_page_offset_bytes, uint32_t mesh_alloc_order_offset_bytes) {
    std::vector<ChunkMeshAlloc> alloc_meta(voxel_grid->count_active_chunks);
    std::vector<uint32_t> dirty_list;
    uint32_t dirty_count;

    voxel_grid->chunk_mesh_alloc_.read_subdata(0, sizeof(ChunkMeshAlloc) * voxel_grid->count_active_chunks, alloc_meta.data());
    voxel_grid->mesh_buffers_status_.read_subdata(sizeof(uint32_t), sizeof(uint32_t), &dirty_count);
    dirty_list.resize(dirty_count);
    voxel_grid->dirty_list_.read_subdata(0, sizeof(uint32_t) * dirty_count, dirty_list.data());
        
    if (dirty_count > 0) {
        std::cout << prefix + " mesh allocs of dirty list:" << std::endl;
        uint32_t count_alloc_pages = 0, count_alloc_states = 0; 
        for (uint32_t dirty_idx = 0; dirty_idx < dirty_count; dirty_idx++) {
            uint32_t chunk_id = dirty_list[dirty_idx];
            uint32_t start_page = *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(&alloc_meta[chunk_id]) + mesh_alloc_page_offset_bytes);
            uint32_t alloc_order = *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(&alloc_meta[chunk_id]) + mesh_alloc_order_offset_bytes);
            if (start_page == INVALID_ID) continue;
            count_alloc_pages += 1u << alloc_order;
            count_alloc_states++;
        }

        std::cout << "ST_ALLOC:     " << "count states = " << count_alloc_states << "   count pages = " << count_alloc_pages << std::endl;
        std::cout << std::endl;

        for (uint32_t dirty_idx = 0; dirty_idx < dirty_count; dirty_idx++) {
            uint32_t chunk_id = dirty_list[dirty_idx];
            uint32_t start_page = *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(&alloc_meta[chunk_id]) + mesh_alloc_page_offset_bytes);
            uint32_t alloc_order = *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(&alloc_meta[chunk_id]) + mesh_alloc_order_offset_bytes);
            if (start_page == INVALID_ID) continue;
            
            std::cout << std::left << std::setw(9 + 8) << ("DIRTY_ID " + std::to_string(dirty_idx))
                    << std::left << std::setw(5) << "  |"
                    << std::left << std::setw(9 + 8) << ("CHUNK_ID " + std::to_string(chunk_id) + ":")
                    << std::right << std::setw(13 + 5) << "start page = "
                    << std::right << std::setw(6 + 5) << start_page
                    << std::right << std::setw(8 + 5) << "order = "
                    << std::right << std::setw(3 + 5) << alloc_order
                    << std::endl;
        }
        std::cout << std::endl;
    } else {
        std::cout << "===DIRTY LIST IS EMPTY===" << std::endl;
        std::cout << std::endl;
    }
}

void VoxelGridGPUDebugger::print_free_lists(
    const BufferObject& heads_buffer, 
    const BufferObject& nodes_buffer, 
    const BufferObject& states_buffer, 
    uint32_t count_nodes, 
    uint32_t count_pages,
    uint32_t max_order) 
{
    std::vector<AllocNode> nodes(count_nodes);
    nodes_buffer.read_subdata(0, sizeof(AllocNode) * count_nodes, nodes.data());

    std::vector<uint32_t> heads(max_order + 1);
    heads_buffer.read_subdata(0, sizeof(uint32_t) * (max_order + 1), heads.data());

    std::vector<uint32_t> states(count_pages);
    states_buffer.read_subdata(0, sizeof(uint32_t) * count_pages, states.data());

    for (uint32_t i = 0; i < max_order + 1; i++) {
        uint32_t order = i;
        std::cout << "======================ORDER " << order << "======================" << std::endl;
        uint32_t head_idx = heads[order] >> HEAD_TAG_BITS;
        uint32_t cur_node = head_idx != INVALID_HEAD_IDX ? head_idx : INVALID_ID;
        while (cur_node != INVALID_ID) {
            uint32_t page_id = nodes[cur_node].page;
            uint32_t kind = states[page_id] & ST_MASK;
            uint32_t real_order = states[page_id] >> ST_MASK_BITS;
            if (real_order == order) {
                std::cout << page_id << " ";
                if (kind == 0u) std::cout << "ST_FREE" << std::endl;
            }
            cur_node = nodes[cur_node].next;
        }
        
        std::cout << std::endl;
    }
}

void VoxelGridGPUDebugger::dispay_debug_window() {
    ImGui::Begin("Debug");

    ImGui::TextUnformatted("Camera position");

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 220, 120, 255));
    ImGui::Text("x: %.3f", window->camera->position.x);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 255, 120, 255));
    ImGui::Text("y: %.3f", window->camera->position.y);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 180, 255, 255));
    ImGui::Text("z: %.3f", window->camera->position.z);
    ImGui::PopStyleColor();

    if (ImGui::Button("Print counters")) {
        print_counters();
        std::cout << "-----------------------" << std::endl << std::endl;
    }
    if (ImGui::Button("Print dirty list")) {
        print_dirty_list();
    }

    if (ImGui::Button("print_dirty_list_quad_count()")) {
        print_dirty_list_quad_count();
    }

    float render_distance_in_chunks = voxel_grid->render_distance / (voxel_grid->voxel_size.x * voxel_grid->chunk_size.x);
    if (ImGui::SliderFloat("Render distance", &render_distance_in_chunks, 0.0f, 300.0f)) {
        voxel_grid->render_distance = render_distance_in_chunks * voxel_grid->voxel_size.x * voxel_grid->chunk_size.x;
    }


    if (ImGui::Button("Print vb free list")) {
        print_free_lists(
            voxel_grid->vb_heads_,
            voxel_grid->vb_nodes_,
            voxel_grid->vb_state_,
            voxel_grid->count_vb_nodes_,
            voxel_grid->count_vb_pages_,
            voxel_grid->vb_order_
        );
    }

    if (ImGui::Button("Print ib free list")) {
        print_free_lists(
            voxel_grid->ib_heads_,
            voxel_grid->ib_nodes_,
            voxel_grid->ib_state_,
            voxel_grid->count_ib_nodes_,
            voxel_grid->count_ib_pages_,
            voxel_grid->ib_order_
        );
    }

    if (ImGui::CollapsingHeader("Dirty list data", 
        ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding)) {
        ImGui::Text("Mesh alloc");

        if (ImGui::Button("Print VB")) {
            print_mesh_alloc_by_dirty_list(
                "VB", 
                offsetof(ChunkMeshAlloc, v_startPage), 
                offsetof(ChunkMeshAlloc, v_order)
            );
        }

        ImGui::SameLine();

        if (ImGui::Button("Print IB")) {
            print_mesh_alloc_by_dirty_list(
                "IB", 
                offsetof(ChunkMeshAlloc, i_startPage), 
                offsetof(ChunkMeshAlloc, i_order)
            );
        }

        ImGui::Separator();
    }

    ImGui::End();
}

void VoxelGridGPUDebugger::display_build_from_dirty_window() {
    ImGui::Begin("Build mesh from dirty pipeline");
    if (ImGui::Button("Run all pipeline")) {
        voxel_grid->build_mesh_from_dirty(math_utils::BITS, math_utils::OFFSET);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Pipeline steps");
    ImGui::Separator();

    if (ImGui::Button("mesh_reset()")) {
        shader_helper->prepare_dispatch_args(voxel_grid->dispatch_args, BufferDispatchArg(&voxel_grid->dirty_list_, 0u));
        voxel_grid->mesh_reset(voxel_grid->dispatch_args);
    }

    if (ImGui::Button("mesh_count()")) {
        uint32_t vox_per_chunk = (uint32_t)(voxel_grid->chunk_size.x * voxel_grid->chunk_size.y * voxel_grid->chunk_size.z);
        shader_helper->prepare_dispatch_args(
            voxel_grid->dispatch_args, 
            ValueDispatchArg(vox_per_chunk), 
            BufferDispatchArg(&voxel_grid->dirty_list_, 0u)
        );
        voxel_grid->mesh_count(voxel_grid->dispatch_args, math_utils::BITS, math_utils::OFFSET);
    }

    if (ImGui::Button("mesh_alloc()")) {
        shader_helper->prepare_dispatch_args(voxel_grid->dispatch_args, BufferDispatchArg(&voxel_grid->dirty_list_, 0u));
        voxel_grid->mesh_alloc(voxel_grid->dispatch_args);
    }

    if (ImGui::Button("verify_mesh_allocation()")) {
        shader_helper->prepare_dispatch_args(voxel_grid->dispatch_args, BufferDispatchArg(&voxel_grid->dirty_list_, 0u));
        voxel_grid->verify_mesh_allocation(voxel_grid->mesh_buffers_status_);
    }

    if (ImGui::Button("return_free_alloc_nodes()")) {
        voxel_grid->prepare_return_free_alloc_nodes(voxel_grid->mesh_buffers_status_);
        voxel_grid->return_free_alloc_nodes(voxel_grid->mesh_buffers_status_);
    }

    if (ImGui::Button("mesh_emit()")) {
        uint32_t vox_per_chunk = (uint32_t)(voxel_grid->chunk_size.x * voxel_grid->chunk_size.y * voxel_grid->chunk_size.z);
        shader_helper->prepare_dispatch_args(
            voxel_grid->dispatch_args, 
            ValueDispatchArg(vox_per_chunk), 
            BufferDispatchArg(&voxel_grid->dirty_list_, 0u)
        );
        voxel_grid->mesh_emit(voxel_grid->dispatch_args, math_utils::BITS, math_utils::OFFSET);
    }

    if (ImGui::Button("mesh_finalize()")) {
        shader_helper->prepare_dispatch_args(voxel_grid->dispatch_args, BufferDispatchArg(&voxel_grid->dirty_list_, 0u));
        voxel_grid->mesh_finalize(voxel_grid->dispatch_args);
    }

    if (ImGui::Button("reset_dirty_count()")) {
        voxel_grid->reset_dirty_count();
    }
    ImGui::End();
}

void VoxelGridGPUDebugger::display_build_cmd_window() {
    ImGui::Begin("Build draw commands pipeline");
    if (ImGui::Button("Run all pipeline")) {
        float aspect = window->get_fbuffer_aspect_ratio();
        glm::mat4 view_matrix = window->camera->get_view_matrix();
        glm::mat4 proj_matrix = window->camera->get_projection_matrix(aspect);
        glm::mat4 view_proj_matrix = proj_matrix * view_matrix;
        voxel_grid->build_indirect_draw_commands_frustum(view_proj_matrix, window->camera->position, math_utils::BITS, math_utils::OFFSET);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Pipeline steps");
    ImGui::Separator();

    if (ImGui::Button("reset_cmd_count()")) {
        voxel_grid->mesh_buffers_status_.update_subdata_fill<uint32_t>(2, 0u, sizeof(uint32_t), *voxel_grid->shader_manager);
    }

    if (ImGui::Button("build_draw_commands()")) {
        float aspect = window->get_fbuffer_aspect_ratio();
        glm::mat4 view_matrix = window->camera->get_view_matrix();
        glm::mat4 proj_matrix = window->camera->get_projection_matrix(aspect);
        glm::mat4 view_proj_matrix = proj_matrix * view_matrix;
        voxel_grid->build_draw_commands(view_proj_matrix, window->camera->position, math_utils::BITS, math_utils::OFFSET);
    }
    ImGui::End();
}

void VoxelGridGPUDebugger::display_draw_pipline_window() {
    ImGui::Begin("Voxel grid draw pipeline");
    if (ImGui::BeginTable("pipeline_table", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("Step");
        ImGui::TableSetupColumn("Streaming", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Run", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableHeadersRow();

        for (uint32_t i = 0; i < COUNT_DRAWING_STEPS; i++) {
            ImGui::TableNextRow(); // ----
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(voxel_grid_draw_steps_names[i].c_str());

            ImGui::TableNextColumn();
            ImGui::PushID(i);

            ImGui::Checkbox("streaming", &voxel_grid_draw_streaming[i]);
            
            ImGui::TableNextColumn();
            if (ImGui::Button("Run once")) {
                voxel_grid_draw_steps[i]();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    for (uint32_t i = 0; i < COUNT_DRAWING_STEPS; i++) {
        if (voxel_grid_draw_streaming[i]) {
            voxel_grid_draw_steps[i]();
        }
    }
    ImGui::End();
}

void VoxelGridGPUDebugger::display_chunk_eviction_window() {
    ImGui::Begin("Chunk enviction");

    if (ImGui::Button("Run all pipeline")) voxel_grid->ensure_free_chunks_gpu(window->camera->position, math_utils::BITS, math_utils::OFFSET);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Debug");
    ImGui::Separator();

    if (ImGui::Button("print eviction log")) print_eviction_log(window->camera->position);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Pipeline steps");
    ImGui::Separator();

    if (ImGui::Button("reset_heads()")) voxel_grid->reset_heads();
    if (ImGui::Button("build_bucket_lists()")) voxel_grid->build_bucket_lists(window->camera->position);
    if (ImGui::Button("evict_lowpriority_chunks()")) {
        voxel_grid->prepare_evict_lowpriority_chunks(voxel_grid->dispatch_args);
        voxel_grid->evict_lowpriority_chunks(voxel_grid->dispatch_args);
    }
    if (ImGui::Button("free_evicted_chunks_mesh()"))  {
        voxel_grid->prepare_evict_lowpriority_chunks(voxel_grid->dispatch_args);
        voxel_grid->free_evicted_chunks_mesh(voxel_grid->dispatch_args);
    }

    if (ImGui::Button("reset_evicted_list_and_buckets()"))  {
        voxel_grid->reset_evicted_list_and_buckets();
    }

    if (ImGui::Button("return_free_alloc_nodes()")) {
        voxel_grid->prepare_return_free_alloc_nodes(voxel_grid->dispatch_args);
        voxel_grid->return_free_alloc_nodes(voxel_grid->dispatch_args);
    }

    if (ImGui::Button("rebuild_chunk_hash_table()")) {
        voxel_grid->rebuild_chunk_hash_table(math_utils::BITS, math_utils::OFFSET);
    }

    ImGui::End();
}

void VoxelGridGPUDebugger::display_stream_chunks_pipeline_window() {
    ImGui::Begin("Steam chunks pipeline");
    
    if (ImGui::Button("Run all pipeline")) {
        voxel_grid->stream_chunks_sphere(window->camera->position, -1, 45345345u);
    }

    ImGui::Separator();
    ImGui::TextDisabled("Pipeline steps");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::BeginTable("pipeline_table", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("Step");
        ImGui::TableSetupColumn("Streaming", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Run", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableHeadersRow();

        for (uint32_t i = 0; i < COUNT_GENERATION_STEPS; i++) {
            ImGui::TableNextRow(); // ----
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(voxel_grid_generation_steps_names[i].c_str());

            ImGui::TableNextColumn();
            ImGui::PushID(i);

            ImGui::Checkbox("streaming", &voxel_grid_generation_streaming[i]);
            
            ImGui::TableNextColumn();
            if (ImGui::Button("Run once")) {
                voxel_grid_generation_steps[i]();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    for (uint32_t i = 0; i < COUNT_GENERATION_STEPS; i++) {
        if (voxel_grid_generation_streaming[i]) {
            voxel_grid_generation_steps[i]();
        }
    }
    ImGui::End();
}

void VoxelGridGPUDebugger::display_hash_table_window() {
    ImGui::Begin("Hash table");

    if (ImGui::Button("Print hash table log")) {
        print_chunks_hash_table_log();
    }

    ImGui::Separator();

    static glm::ivec3 chunk_pos(0);
    ImGui::InputInt3("Chunk pos", &chunk_pos.x);
    if (ImGui::Button("Find hash table matches"))
        print_finded_chunks_in_hash_table(chunk_pos);
    
    ImGui::End();
}
