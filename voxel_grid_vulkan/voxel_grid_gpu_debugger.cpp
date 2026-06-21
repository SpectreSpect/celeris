#include "voxel_grid_gpu_debugger.h"

#include <unordered_set>
#include <vector>
#include <cstddef>

#include "shader_helper/buffer_dispatch_arg.h"
#include "voxel_grid_structures.h"
#include "../camera/camera.h"
#include "../imgui_layer.h"
#include "../math_utils.h"
#include "voxel_grid.h"
#include "../vulkan_self/vulkan_queue.h"
#include "../vulkan_self/vulkan_device.h"

VoxelGridGPUDebugger::VoxelGridGPUDebugger(
    VoxelGrid& voxel_grid,
    VulkanDevice& device,
    VulkanQueue& queue,
    const Window& window,
    const Camera& camera,
    bool default_tasks_activity_state)
    :   m_voxel_grid(&voxel_grid),
        m_queue(&queue),
        m_command_pool(device, *m_queue),
        m_command_buffer(device, m_command_pool),
        m_fence(device),
        m_draw_tasks(create_draw_tasks(window, camera, default_tasks_activity_state)),
        m_generation_tasks(create_generation_tasks(camera, default_tasks_activity_state)) {}

VulkanCommandBuffer& VoxelGridGPUDebugger::command_buffer() noexcept {
    return m_command_buffer;
}

void VoxelGridGPUDebugger::print_found_chunks_in_hash_table(glm::ivec3 chunk_pos) {
    std::vector<ChunkHashTableSlot> chunk_hash_table_slot(m_voxel_grid->params().chunk_hash_table_size);
    m_voxel_grid->buffers().chunk_hash_table.read(
        chunk_hash_table_slot.data(), 
        sizeof(ChunkHashTableSlot) * m_voxel_grid->params().chunk_hash_table_size, 
        sizeof(uint32_t) * 2);

    uint64_t key = math_utils::pack_key(chunk_pos.x, chunk_pos.y, chunk_pos.z);
    
    uint32_t count_matches = 0;
    std::cout << "==== HASH TABLE (" << 
                 chunk_pos.x << ", " << 
                 chunk_pos.y << ", " << 
                 chunk_pos.z << ") MATCHES ====" << std::endl;
    
    for (uint32_t slot_id = 0; slot_id < m_voxel_grid->params().chunk_hash_table_size; slot_id++) {
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
    VulkanBuffer& load_list = m_voxel_grid->buffers().load_list;
    VulkanBuffer& voxel_write_list = m_voxel_grid->buffers().voxel_write_list;
    VulkanBuffer& dirty_list = m_voxel_grid->buffers().dirty_list;
    VulkanBuffer& indirect_cmds = m_voxel_grid->buffers().indirect_cmds;
    VulkanBuffer& free_list = m_voxel_grid->buffers().free_list;
    VulkanBuffer& failed_dirty_list = m_voxel_grid->buffers().failed_dirty_list;
    VulkanBuffer& mesh_buffers_status = m_voxel_grid->buffers().mesh_buffers_status;
    VulkanBuffer& vb_free_nodes_list = m_voxel_grid->buffers().vb_mesh_allocator_buffers.free_nodes_list;
    VulkanBuffer& ib_free_nodes_list = m_voxel_grid->buffers().ib_mesh_allocator_buffers.free_nodes_list;
    VulkanBuffer& debug_counter = m_voxel_grid->buffers().debug_counter;

    uint32_t load_list_count = load_list.read_scalar<uint32_t>(0u);
    uint32_t write_count = voxel_write_list.read_scalar<uint32_t>(0);
    uint32_t dirty_count = dirty_list.read_scalar<uint32_t>(0);
    uint32_t cmd_count = indirect_cmds.read_scalar<uint32_t>(0);
    uint32_t free_count = free_list.read_scalar<uint32_t>(0);
    uint32_t failed_dirty_count = failed_dirty_list.read_scalar<uint32_t>(0);
    uint32_t is_vb_full = mesh_buffers_status.read_scalar<uint32_t>(0);
    uint32_t is_ib_full = mesh_buffers_status.read_scalar<uint32_t>(sizeof(uint32_t));
    std::vector<uint32_t> mesh_alloc_counters = debug_counter.read_vector<uint32_t>(3);

    std::cout << "write_count: " << write_count << std::endl;
    std::cout << "dirty_count: " << dirty_count << std::endl;
    std::cout << "cmd_count: " << cmd_count << std::endl;
    std::cout << "free_count: " << free_count << std::endl;
    std::cout << "failed_dirty_count: " << failed_dirty_count << std::endl;
    std::cout << "is_vb_full: " << (is_vb_full == 1u ? "TRUE" : "FALSE") << std::endl;
    std::cout << "count_ib_free_pages: " << (is_ib_full == 1u ? "TRUE" : "FALSE") << std::endl;
    std::cout << "load_list_count: " << load_list_count << std::endl;

    uint32_t count_free_nodes_vb = vb_free_nodes_list.read_scalar<uint32_t>(0);
    uint32_t count_free_nodes_ib = ib_free_nodes_list.read_scalar<uint32_t>(0);

    std::cout << "count_free_nodes_vb: " << count_free_nodes_vb << std::endl;
    std::cout << "count_free_nodes_ib: " << count_free_nodes_ib << std::endl;
    
    std::cout << std::endl;

    std::cout << "=== Mesh alloc debug counter ===" << std::endl;
    std::cout << "push buddy errors: "  << mesh_alloc_counters[0] << std::endl;
    std::cout << "zero attempts limit error: "  << mesh_alloc_counters[1] << std::endl;
    std::cout << "out of while loop error: "  << mesh_alloc_counters[2] << std::endl;
    std::cout << std::endl;

    print_count_free_mesh_alloc();
}

void VoxelGridGPUDebugger::print_count_free_mesh_alloc() {
    std::vector<ChunkMeshAlloc> alloc_meta(m_voxel_grid->params().count_active_chunks);
    std::vector<uint32_t> vb_states(m_voxel_grid->params().vb_allocator_params.count_pages);
    std::vector<uint32_t> ib_states(m_voxel_grid->params().ib_allocator_params.count_pages);
    std::vector<uint32_t> vb_heads(m_voxel_grid->params().vb_allocator_params.max_order + 1);
    std::vector<uint32_t> ib_heads(m_voxel_grid->params().ib_allocator_params.max_order + 1);
    std::vector<AllocNode> vb_nodes(m_voxel_grid->params().vb_allocator_params.count_nodes);
    std::vector<AllocNode> ib_nodes(m_voxel_grid->params().ib_allocator_params.count_nodes);
    std::vector<uint32_t> dirty_list;
    uint32_t dirty_count;

    m_voxel_grid->buffers().chunk_mesh_alloc.read(alloc_meta.data(), sizeof(ChunkMeshAlloc) * m_voxel_grid->params().count_active_chunks, 0);
    m_voxel_grid->buffers().vb_mesh_allocator_buffers.state.read(vb_states.data(), sizeof(uint32_t) * m_voxel_grid->params().vb_allocator_params.count_pages,  0);
    m_voxel_grid->buffers().ib_mesh_allocator_buffers.state.read(ib_states.data(), sizeof(uint32_t) * m_voxel_grid->params().ib_allocator_params.count_pages,  0);
    m_voxel_grid->buffers().vb_mesh_allocator_buffers.heads.read(vb_heads.data(), sizeof(uint32_t) * (m_voxel_grid->params().vb_allocator_params.max_order + 1), 0);
    m_voxel_grid->buffers().ib_mesh_allocator_buffers.heads.read(ib_heads.data(), sizeof(uint32_t) * (m_voxel_grid->params().ib_allocator_params.max_order + 1), 0);
    m_voxel_grid->buffers().vb_mesh_allocator_buffers.nodes.read(vb_nodes.data(), sizeof(AllocNode) * m_voxel_grid->params().vb_allocator_params.count_nodes, 0);
    m_voxel_grid->buffers().ib_mesh_allocator_buffers.nodes.read(ib_nodes.data(), sizeof(AllocNode) * m_voxel_grid->params().ib_allocator_params.count_nodes, 0);
    m_voxel_grid->buffers().dirty_list.read(&dirty_count, sizeof(uint32_t), 0);
    dirty_list.resize(dirty_count);
    m_voxel_grid->buffers().dirty_list.read(dirty_list.data(), sizeof(uint32_t) * dirty_count, sizeof(uint32_t));

    //=================РАСЧЁТ ДАННЫХ ПО MESH_ALLOC=================
    std::unordered_set<uint32_t> allocated_mesh;
    uint32_t count_alloc_vb_pages_from_meta = 0, count_alloc_ib_pages_from_meta = 0;
    uint32_t count_vb_alloc_chunks_from_meta = 0, count_ib_alloc_chunks_from_meta = 0;
    for (uint32_t i = 0; i < m_voxel_grid->params().count_active_chunks; i++) {
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
    for (uint32_t i = 0; i < m_voxel_grid->params().vb_allocator_params.count_pages; i++) {
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
    for (uint32_t i = 0; i < m_voxel_grid->params().ib_allocator_params.count_pages; i++) {
        uint32_t kind = ib_states[i] & ST_MASK;
        uint32_t order = ib_states[i] >> ST_MASK_BITS;
        uint32_t count_pages = 1u << order;
        if (kind == ST_FREE) {count_ib_free++; ib_free_pages += count_pages; }
        if (kind == ST_ALLOC) {count_ib_alloc++; ib_alloc_pages += count_pages; }
        if (kind == ST_MERGED) {count_ib_merged++; ib_merged_pages += count_pages; }
    }

    //=================РАСЧЁТ ДАННЫХ VB ПО HEADS=================
    std::vector<uint32_t> count_free_states_by_vb_order(m_voxel_grid->params().vb_allocator_params.max_order + 1, 0);
    std::vector<uint32_t> count_free_pages_by_vb_order(m_voxel_grid->params().vb_allocator_params.max_order + 1, 0);
    uint32_t count_free_states_by_vb_heads = 0, count_free_pages_by_vb_heads = 0;
    for (uint32_t order = 0; order <= m_voxel_grid->params().vb_allocator_params.max_order; order++) {
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
    std::vector<uint32_t> count_free_states_by_ib_order(m_voxel_grid->params().ib_allocator_params.max_order + 1, 0);
    std::vector<uint32_t> count_free_pages_by_ib_order(m_voxel_grid->params().ib_allocator_params.max_order + 1, 0);
    uint32_t count_free_states_by_ib_heads = 0, count_free_pages_by_ib_heads = 0;
    for (uint32_t order = 0; order <= m_voxel_grid->params().ib_allocator_params.max_order; order++) {
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
    std::cout << "REAL_COUNT_PAGES: " << m_voxel_grid->params().vb_allocator_params.count_pages << std::endl;
    std::cout << "LIMBO: " << (int)m_voxel_grid->params().vb_allocator_params.count_pages - (vb_free_pages + vb_alloc_pages) << std::endl;
    std::cout << std::endl;
    std::cout << "==Index buffer==" << std::endl;
    std::cout << "ST_FREE:      count states = " << count_ib_free     << "  count pages = " << ib_free_pages << std::endl;
    std::cout << "ST_ALLOC:     count states = " << count_ib_alloc    << "  count pages = " << ib_alloc_pages << std::endl;
    std::cout << "ST_MERGED:    count states = " << count_ib_merged   << "  count pages = " << ib_merged_pages << std::endl;
    std::cout << "ST_MERGING:   count states = " << count_ib_merging  << "  count pages = " << ib_merging_pages << std::endl;
    std::cout << "ST_READY:     count states = " << count_ib_ready    << "  count pages = " << ib_ready_pages << std::endl;
    std::cout << "ST_CONCEDED:  count states = " << count_ib_conceded << "  count pages = " << ib_conceded_pages << std::endl;
    std::cout << "SUM (free + alloc): count states = " << count_ib_free + count_ib_alloc << "  count pages = " << ib_free_pages + ib_alloc_pages << std::endl;
    std::cout << "REAL_COUNT_PAGES: " << m_voxel_grid->params().ib_allocator_params.count_pages << std::endl;
    std::cout << "LIMBO: " << (int)m_voxel_grid->params().ib_allocator_params.count_pages - (ib_free_pages + ib_alloc_pages) << std::endl;
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
    for (uint32_t order = 0; order <= m_voxel_grid->params().vb_allocator_params.max_order; order++) {
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
    for (uint32_t order = 0; order <= m_voxel_grid->params().ib_allocator_params.max_order; order++) {
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
    std::vector<ChunkHashTableSlot> chunk_hash_table_slot(m_voxel_grid->params().chunk_hash_table_size);
    m_voxel_grid->buffers().chunk_hash_table.read(chunk_hash_table_slot.data(), sizeof(ChunkHashTableSlot) * m_voxel_grid->params().chunk_hash_table_size, sizeof(HashTableCounters));

    HashTableCounters counters = m_voxel_grid->buffers().chunk_hash_table.read_scalar<HashTableCounters>(0u);

    uint32_t count_empty_slots = 0u, 
             count_lock_slots = 0u, 
             count_tomb_slots = 0u, 
             count_occupied_slots = 0u, 
             count_error_states = 0u; 
    for (uint32_t slot_id = 0u; slot_id < m_voxel_grid->params().chunk_hash_table_size; slot_id++) {
        uint32_t state = chunk_hash_table_slot[slot_id].state;

        if (state == SLOT_EMPTY) count_empty_slots++;
        else if (state == SLOT_LOCKED) count_lock_slots++;
        else if (state == SLOT_TOMB) count_tomb_slots++;
        else if (state == SLOT_OCCUPIED) count_occupied_slots++;
        else count_error_states++;
    }

    auto get_count_string = [&](uint32_t count_cpu) -> std::string {
        std::ostringstream ss;

        float cpu_percent = static_cast<float>(count_cpu) / m_voxel_grid->params().chunk_hash_table_size * 100.0f;

        ss << count_cpu
        << " (" << std::fixed << std::setprecision(2) << cpu_percent << "%)";

        return ss.str();
    };

    std::cout << "======= CHUNKS HASH TABLE LOG =======" << std::endl;
    std::cout << "Total count hash table slots: " << m_voxel_grid->params().chunk_hash_table_size << std::endl;

    std::cout << "**CPU**:" << std::endl;
    std::cout << "SLOT_EMPTY: " << get_count_string(count_empty_slots) << std::endl;
    std::cout << "SLOT_TOMB: " << get_count_string(count_tomb_slots) << std::endl;
    std::cout << "SLOT_OCCUPIED: " << get_count_string(count_occupied_slots) << std::endl;
    std::cout << "SLOT_LOCKED: " << count_lock_slots << "(" 
              << std::fixed << std::setprecision(2) << (float)count_lock_slots / m_voxel_grid->params().chunk_hash_table_size * 100.0f << "%)" << std::endl;

    std::cout << "ERROR_SLOTS: " << count_error_states << "(" 
              << std::fixed << std::setprecision(2) << (float)count_error_states / m_voxel_grid->params().chunk_hash_table_size * 100.0f << "%)" << std::endl;
    std::cout << std::endl;

    std::cout << "**GPU**:" << std::endl;
    std::cout << "SLOT_EMPTY: " << get_count_string(counters.reduce_read_count_empty()) << std::endl;
    std::cout << "SLOT_TOMB: " << get_count_string(counters.reduce_read_count_tomb()) << std::endl;
    std::cout << "SLOT_OCCUPIED: " << get_count_string(counters.reduce_read_count_occupied()) << std::endl;

    std::cout << std::endl;
}

void VoxelGridGPUDebugger::print_eviction_log(const glm::vec3& camera_pos) {
    std::vector<BucketHead> bucket_heads(m_voxel_grid->params().count_evict_buckets);
    std::vector<uint32_t> bucket_next(m_voxel_grid->params().count_active_chunks);
    std::vector<ChunkMetaGPU> chunk_meta(m_voxel_grid->params().count_active_chunks);

    m_voxel_grid->buffers().bucket_heads.read(bucket_heads.data(), sizeof(BucketHead) * m_voxel_grid->params().count_evict_buckets, 0);
    m_voxel_grid->buffers().bucket_next.read(bucket_next.data(), sizeof(uint32_t) * m_voxel_grid->params().count_active_chunks, 0);
    m_voxel_grid->buffers().chunk_meta.read(chunk_meta.data(), sizeof(ChunkMetaGPU) * m_voxel_grid->params().count_active_chunks, 0);

    struct ChunkInBucketData {
        uint32_t chunk_id;
        glm::ivec3 coords;
        double distance_to_chunk;
        uint32_t bucket_id_by_distance;
    };
    
    // ==================Подсчёт по HEADS==================
    std::vector<uint32_t> count_chunks_per_bucket(m_voxel_grid->params().count_evict_buckets, 0u);
    std::vector<uint32_t> count_chunk_mismatches_per_bucket(m_voxel_grid->params().count_evict_buckets, 0u);
    uint32_t total_chunks_number_in_buckets = 0u, total_chunk_mismatches_in_buckets = 0u;
    std::vector<std::vector<ChunkInBucketData>> chunks_per_bucket(m_voxel_grid->params().count_evict_buckets);
    for (uint32_t bucket_id = 0; bucket_id < m_voxel_grid->params().count_evict_buckets; bucket_id++) {
        uint32_t cur_id = bucket_heads[bucket_id].id;
        
        while (cur_id != INVALID_ID) {
            count_chunks_per_bucket[bucket_id]++;
            total_chunks_number_in_buckets++;

            ChunkInBucketData chunk_in_bucket;
            chunk_in_bucket.chunk_id = cur_id;

            uint64_t coords_key = ((uint64_t)(chunk_meta[cur_id].key_hi) << 32u) | (uint64_t)(chunk_meta[cur_id].key_lo);
            chunk_in_bucket.coords = math_utils::unpack_key(coords_key);

            glm::vec3 render_chunk_pos = glm::vec3(glm::vec3(chunk_in_bucket.coords) * glm::vec3(m_voxel_grid->params().chunk_size)) * glm::vec3(m_voxel_grid->voxel_size());
            glm::vec3 render_chunk_center = render_chunk_pos + glm::vec3(0.5) * glm::vec3(m_voxel_grid->params().chunk_size) * glm::vec3(m_voxel_grid->voxel_size());
            chunk_in_bucket.distance_to_chunk = glm::length(render_chunk_center - camera_pos);
            chunk_in_bucket.bucket_id_by_distance = (uint32_t)(chunk_in_bucket.distance_to_chunk / m_voxel_grid->params().eviction_bucket_shell_thickness);

            chunks_per_bucket[bucket_id].push_back(chunk_in_bucket);

            if (bucket_id != chunk_in_bucket.bucket_id_by_distance) {
                count_chunk_mismatches_per_bucket[bucket_id]++;
                total_chunk_mismatches_in_buckets++;
            }

            cur_id = bucket_next[cur_id];
        }
        
        std::cout << std::endl;
    }

    std::vector<double> min_distance_in_shell(m_voxel_grid->params().count_evict_buckets, std::numeric_limits<double>::max());
    std::vector<double> max_distance_in_shell(m_voxel_grid->params().count_evict_buckets, 0.0);
    for (uint32_t bucket_id = 0; bucket_id < m_voxel_grid->params().count_evict_buckets; bucket_id++) {
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
    
    for (uint32_t bucket_id = 0u; bucket_id < m_voxel_grid->params().count_evict_buckets; bucket_id++) {
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
    uint32_t dirty_count = m_voxel_grid->buffers().dirty_list.read_scalar<uint32_t>(0u);
    std::vector<uint32_t> dirty_list(dirty_count);
    m_voxel_grid->buffers().dirty_list.read(dirty_list.data(), sizeof(uint32_t) * dirty_count, sizeof(uint32_t));

    std::cout << "DIRTY_LIST: " << std::endl;
    for (uint32_t dirty_id = 0u; dirty_id < dirty_count && dirty_id < 100u; dirty_id++) {
        std::cout << "dirty_id " << dirty_id << ": " << dirty_list[dirty_id] << std::endl;
    }
    std::cout << std::endl;
}

void VoxelGridGPUDebugger::print_dirty_list_emit_counters() {
    uint32_t dirty_count = m_voxel_grid->buffers().dirty_list.read_scalar<uint32_t>(0u);
    std::vector<uint32_t> dirty_list(dirty_count);
    std::vector<uint32_t> emit_counters(m_voxel_grid->params().count_active_chunks);
    
    m_voxel_grid->buffers().dirty_list.read(dirty_list.data(), sizeof(uint32_t) * dirty_count, sizeof(uint32_t));
    m_voxel_grid->buffers().emit_counters.read(emit_counters.data(), sizeof(uint32_t) * m_voxel_grid->params().count_active_chunks, 0);

    std::cout << "EMIT COUNTERS: " << std::endl;
    for (uint32_t dirty_id = 0u; dirty_id < dirty_count && dirty_id < 100u; dirty_id++) {
        uint32_t chunk_id = dirty_list[dirty_id];
        std::cout << "dirty_id " << dirty_id << " chunk_id " << chunk_id << ": " << emit_counters[chunk_id] << std::endl;
    }
    std::cout << std::endl;
}

void VoxelGridGPUDebugger::print_dirty_list_quad_count() {
    uint32_t dirty_count = m_voxel_grid->buffers().dirty_list.read_scalar<uint32_t>(0u);
    std::vector<uint32_t> dirty_quad_count(dirty_count);
    
    m_voxel_grid->buffers().dirty_quad_count.read(dirty_quad_count.data(), sizeof(uint32_t) * dirty_count, 0);

    uint32_t total_quad_count = 0u;
    for (uint32_t quad_count : dirty_quad_count) {
        total_quad_count += quad_count;
    }

    std::cout << "TOTAL QUAD COUNT: " << total_quad_count << std::endl;
    std::cout << "DIRTY COUNT: " << dirty_count << std::endl;
    std::cout << "DIRTY QUAD COUNTERS: " << std::endl;
    for (uint32_t dirty_id = 0u; dirty_id < dirty_count && dirty_id < 100u; dirty_id++) {
        std::cout << "dirty_id " << dirty_id << ": " << dirty_quad_count[dirty_id] << std::endl;
    }
    std::cout << std::endl;
}

void VoxelGridGPUDebugger::print_mesh_alloc_by_dirty_list(
        const std::string& prefix, 
        uint32_t mesh_alloc_page_offset_bytes, 
        uint32_t mesh_alloc_order_offset_bytes) {
    std::vector<ChunkMeshAlloc> alloc_meta(m_voxel_grid->params().count_active_chunks);
    std::vector<uint32_t> dirty_list;
    uint32_t dirty_count;

    m_voxel_grid->buffers().chunk_mesh_alloc.read(alloc_meta.data(), sizeof(ChunkMeshAlloc) * m_voxel_grid->params().count_active_chunks, 0);
    m_voxel_grid->buffers().mesh_buffers_status.read(&dirty_count, sizeof(uint32_t), sizeof(uint32_t));
    dirty_list.resize(dirty_count);
    m_voxel_grid->buffers().dirty_list.read(dirty_list.data(), sizeof(uint32_t) * dirty_count, 0);
        
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
        VulkanBuffer& heads_buffer,
        VulkanBuffer& nodes_buffer,
        VulkanBuffer& states_buffer,
        uint32_t count_nodes,
        uint32_t count_pages,
        uint32_t max_order){
    std::vector<AllocNode> nodes(count_nodes);
    nodes_buffer.read(nodes.data(), sizeof(AllocNode) * count_nodes, 0);

    std::vector<uint32_t> heads(max_order + 1);
    heads_buffer.read(heads.data(), sizeof(uint32_t) * (max_order + 1), 0);

    std::vector<uint32_t> states(count_pages);
    states_buffer.read(states.data(), sizeof(uint32_t) * count_pages, 0);

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

void VoxelGridGPUDebugger::dispay_debug_window(const Camera& camera) {
    ImGui::Begin("Debug");

    ImGui::TextUnformatted("Camera position");

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 220, 120, 255));
    ImGui::Text("x: %.3f", camera.position.x);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 255, 120, 255));
    ImGui::Text("y: %.3f", camera.position.y);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 180, 255, 255));
    ImGui::Text("z: %.3f", camera.position.z);
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

    float render_distance_in_chunks = m_voxel_grid->params().render_distance / (m_voxel_grid->voxel_size().x * m_voxel_grid->params().chunk_size.x);
    if (ImGui::SliderFloat("Render distance", &render_distance_in_chunks, 0.0f, 300.0f)) {
        m_voxel_grid->set_render_distance(render_distance_in_chunks * m_voxel_grid->voxel_size().x * m_voxel_grid->params().chunk_size.x);
    }


    if (ImGui::Button("Print vb free list")) {
        print_free_lists(
            m_voxel_grid->buffers().vb_mesh_allocator_buffers.heads,
            m_voxel_grid->buffers().vb_mesh_allocator_buffers.nodes,
            m_voxel_grid->buffers().vb_mesh_allocator_buffers.state,
            m_voxel_grid->params().vb_allocator_params.count_nodes,
            m_voxel_grid->params().vb_allocator_params.count_pages,
            m_voxel_grid->params().vb_allocator_params.max_order
        );
    }

    if (ImGui::Button("Print ib free list")) {
        print_free_lists(
            m_voxel_grid->buffers().ib_mesh_allocator_buffers.heads,
            m_voxel_grid->buffers().ib_mesh_allocator_buffers.nodes,
            m_voxel_grid->buffers().ib_mesh_allocator_buffers.state,
            m_voxel_grid->params().ib_allocator_params.count_nodes,
            m_voxel_grid->params().ib_allocator_params.count_pages,
            m_voxel_grid->params().ib_allocator_params.max_order
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

void VoxelGridGPUDebugger::display_build_from_dirty_window(VulkanCommandBuffer& command_buffer) {
    ShaderHelper& shader_helper = m_voxel_grid->shader_helper();
    uint32_t vox_per_chunk = static_cast<uint32_t>(m_voxel_grid->vox_per_chunk());

    ImGui::Begin("Build mesh from dirty pipeline");
    if (ImGui::Button("Run all pipeline")) {
        m_voxel_grid->build_mesh_from_dirty(
            command_buffer, 
            math_utils::BITS, 
            math_utils::OFFSET);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Pipeline steps");
    ImGui::Separator();

    if (ImGui::Button("mesh_reset()")) {
        shader_helper.prepare_dispatch_args(
            command_buffer, 
            m_voxel_grid->buffers().dispatch_args, 
            BufferDispatchArg(&m_voxel_grid->buffers().dirty_list, 0u)
        );
        m_voxel_grid->mesh_reset(command_buffer, m_voxel_grid->buffers().dispatch_args);
    }

    if (ImGui::Button("mesh_count()")) {
        shader_helper.prepare_dispatch_args(
            command_buffer,
            m_voxel_grid->buffers().dispatch_args, 
            ValueDispatchArg(vox_per_chunk), 
            BufferDispatchArg(&m_voxel_grid->buffers().dirty_list, 0u)
        );
        m_voxel_grid->mesh_count(
            command_buffer, 
            m_voxel_grid->buffers().dispatch_args, 
            math_utils::BITS, 
            math_utils::OFFSET
        );
    }

    if (ImGui::Button("mesh_alloc()")) {
        m_voxel_grid->mesh_alloc(
            command_buffer,
            m_voxel_grid->m_buffers.dispatch_args,
            m_voxel_grid->m_buffers.vb_mesh_allocator_buffers,
            m_voxel_grid->m_params.vb_allocator_params,
            4u * sizeof(VertexGPU),
            true
        );

        m_voxel_grid->mesh_alloc(
            command_buffer,
            m_voxel_grid->m_buffers.dispatch_args,
            m_voxel_grid->m_buffers.ib_mesh_allocator_buffers,
            m_voxel_grid->m_params.ib_allocator_params,
            6u * sizeof(uint32_t),
            false
        );
    }

    if (ImGui::Button("verify_mesh_allocation()")) {
        shader_helper.prepare_dispatch_args(
            command_buffer,
            m_voxel_grid->buffers().dispatch_args, 
            BufferDispatchArg(&m_voxel_grid->buffers().dirty_list, 0u)
        );
        m_voxel_grid->verify_mesh_allocation(
            command_buffer,
            m_voxel_grid->buffers().dispatch_args
        );
    }

    if (ImGui::Button("return_free_alloc_nodes()")) {
        m_voxel_grid->prepare_return_free_alloc_nodes(
            command_buffer,
            m_voxel_grid->buffers().dispatch_args
        );
        m_voxel_grid->return_free_alloc_nodes(
            command_buffer,
            m_voxel_grid->buffers().dispatch_args
        );
    }

    if (ImGui::Button("mesh_emit()")) {
        shader_helper.prepare_dispatch_args(
            command_buffer,
            m_voxel_grid->buffers().dispatch_args, 
            ValueDispatchArg(vox_per_chunk), 
            BufferDispatchArg(&m_voxel_grid->buffers().dirty_list, 0u)
        );
        m_voxel_grid->mesh_emit(
            command_buffer, 
            m_voxel_grid->buffers().dispatch_args, 
            math_utils::BITS, 
            math_utils::OFFSET);
    }

    if (ImGui::Button("mesh_finalize()")) {
        shader_helper.prepare_dispatch_args(
            command_buffer,
            m_voxel_grid->buffers().dispatch_args, 
            BufferDispatchArg(&m_voxel_grid->buffers().dirty_list, 0u)
        );
        m_voxel_grid->mesh_finalize(
            command_buffer, 
            m_voxel_grid->buffers().dispatch_args
        );
    }

    if (ImGui::Button("reset_dirty_count()")) {
        m_voxel_grid->reset_dirty_count(command_buffer);
    }
    ImGui::End();
}

void VoxelGridGPUDebugger::display_build_cmd_window(VulkanCommandBuffer& command_buffer, Window& window, const Camera& camera) {
    ShaderHelper& shader_helper = m_voxel_grid->shader_helper();

    ImGui::Begin("Build draw commands pipeline");
    if (ImGui::Button("Run all pipeline")) {
        float aspect = float(window.width()) / float(window.height());
        glm::mat4 view_matrix = camera.get_view_matrix();
        glm::mat4 proj_matrix = camera.get_projection_matrix(aspect);
        glm::mat4 view_proj_matrix = proj_matrix * view_matrix;

        m_voxel_grid->build_indirect_draw_commands_frustum(
            command_buffer, 
            view_proj_matrix, 
            camera.position, 
            math_utils::BITS, 
            math_utils::OFFSET
        );
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Pipeline steps");
    ImGui::Separator();

    if (ImGui::Button("reset_cmd_count()")) {
        m_voxel_grid->buffers().mesh_buffers_status.fill(command_buffer, 0u, sizeof(uint32_t), 2);
    }

    if (ImGui::Button("build_draw_commands()")) {
        float aspect = float(window.width()) / float(window.height());
        glm::mat4 view_matrix = camera.get_view_matrix();
        glm::mat4 proj_matrix = camera.get_projection_matrix(aspect);
        glm::mat4 view_proj_matrix = proj_matrix * view_matrix;
        m_voxel_grid->build_draw_commands(
            command_buffer, 
            view_proj_matrix, 
            camera.position, 
            math_utils::BITS, 
            math_utils::OFFSET
        );
    }
    ImGui::End();
}

void VoxelGridGPUDebugger::display_draw_pipline_window(VulkanCommandBuffer& command_buffer) {
    ImGui::Begin("Voxel grid draw pipeline");
    if (ImGui::BeginTable("pipeline_table", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("Step");
        ImGui::TableSetupColumn("Streaming", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Run", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < m_draw_tasks.size(); i++) {
            ImGui::TableNextRow(); // ----
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(m_draw_tasks[i].name.c_str());

            ImGui::TableNextColumn();
            ImGui::PushID(i);

            ImGui::Checkbox("streaming", &m_draw_tasks[i].is_active);
            
            ImGui::TableNextColumn();
            if (ImGui::Button("Run once")) {
                m_draw_tasks[i].func(command_buffer);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    for (uint32_t i = 0; i < m_draw_tasks.size(); i++) {
        if (m_draw_tasks[i].is_active) {
            m_draw_tasks[i].func(command_buffer);
        }
    }
    ImGui::End();
}

void VoxelGridGPUDebugger::display_chunk_eviction_window(VulkanCommandBuffer& command_buffer, const Camera& camera) {
    ImGui::Begin("Chunk enviction");

    if (ImGui::Button("Run all pipeline")) {
        m_voxel_grid->ensure_free_chunks_gpu(
            command_buffer, 
            camera.position, 
            math_utils::BITS, 
            math_utils::OFFSET
        );
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Debug");
    ImGui::Separator();

    if (ImGui::Button("print eviction log")) 
        print_eviction_log(camera.position);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Pipeline steps");
    ImGui::Separator();

    if (ImGui::Button("reset_heads()")) {
        m_voxel_grid->reset_heads(command_buffer);
    }
    if (ImGui::Button("build_bucket_lists()")) {
        m_voxel_grid->build_bucket_lists(command_buffer, camera.position);
    } 
    if (ImGui::Button("evict_lowpriority_chunks()")) {
        m_voxel_grid->prepare_evict_lowpriority_chunks(
            command_buffer, 
            m_voxel_grid->buffers().dispatch_args
        );
        m_voxel_grid->evict_lowpriority_chunks(
            command_buffer, 
            m_voxel_grid->buffers().dispatch_args
        );
    }
    if (ImGui::Button("free_evicted_chunks_mesh()")) {
        m_voxel_grid->prepare_evict_lowpriority_chunks(
            command_buffer, 
            m_voxel_grid->buffers().dispatch_args
        );
        m_voxel_grid->free_evicted_chunks_mesh(
            command_buffer, 
            m_voxel_grid->buffers().dispatch_args
        );
    }

    if (ImGui::Button("reset_evicted_list_and_buckets()"))  {
        m_voxel_grid->reset_evicted_list_and_buckets(command_buffer);
    }

    if (ImGui::Button("return_free_alloc_nodes()")) {
        m_voxel_grid->prepare_return_free_alloc_nodes(
            command_buffer, 
            m_voxel_grid->buffers().dispatch_args
        );
        m_voxel_grid->return_free_alloc_nodes(
            command_buffer, 
            m_voxel_grid->buffers().dispatch_args
        );
    }

    if (ImGui::Button("rebuild_chunk_hash_table()")) {
        m_voxel_grid->rebuild_chunk_hash_table(
            command_buffer, 
            math_utils::BITS, 
            math_utils::OFFSET
        );
    }

    ImGui::End();
}

void VoxelGridGPUDebugger::display_stream_chunks_pipeline_window(VulkanCommandBuffer& command_buffer, const Camera& camera) {
    ImGui::Begin("Steam chunks pipeline");
    
    if (ImGui::Button("Run all pipeline")) {
        m_voxel_grid->stream_chunks_sphere(
            command_buffer, 
            camera.position, 
            -1, 
            45345345u
        );
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

        for (int i = 0; i < m_generation_tasks.size(); i++) {
            ImGui::TableNextRow(); // ----
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(m_generation_tasks[i].name.c_str());

            ImGui::TableNextColumn();
            ImGui::PushID(i);

            ImGui::Checkbox("streaming", &m_generation_tasks[i].is_active);
            
            ImGui::TableNextColumn();
            if (ImGui::Button("Run once")) {
                m_generation_tasks[i].func(command_buffer);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    for (int i = 0; i < m_generation_tasks.size(); i++) {
        if (m_generation_tasks[i].is_active) {
            m_generation_tasks[i].func(command_buffer);
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
        print_found_chunks_in_hash_table(chunk_pos);
    
    ImGui::End();
}

void VoxelGridGPUDebugger::submit_commands() {
    LOG_METHOD();

    logger.check(m_queue != nullptr, "Queue pointer specify to null");

    m_fence.reset();
    m_queue->submit(m_command_buffer, &m_fence);
    m_fence.wait();
    m_command_buffer.reset();
}

std::vector<VoxelGridGPUDebugger::Task> VoxelGridGPUDebugger::create_draw_tasks(
    const Window& window,
    const Camera& camera,
    bool default_tasks_activity_state)
{
    LOG_METHOD();

    std::vector<Task> tasks = {
        Task{
            "build_mesh_from_dirty()",
            default_tasks_activity_state,
            [&](VulkanCommandBuffer& command_buffer){
                m_voxel_grid->build_mesh_from_dirty(command_buffer, math_utils::BITS, math_utils::OFFSET);
            }
        },
        Task{
            "build_indirect_draw_commands_frustum_fn()",
            default_tasks_activity_state,
            [&](VulkanCommandBuffer& command_buffer){
                float aspect = float(window.width()) / float(window.height());
                glm::mat4 vp = camera.get_projection_matrix(aspect) * camera.get_view_matrix();

                m_voxel_grid->build_indirect_draw_commands_frustum(
                    command_buffer, 
                    vp, 
                    camera.position, 
                    math_utils::BITS, 
                    math_utils::OFFSET
                );
            }
        }
    };

    return tasks;
}

std::vector<VoxelGridGPUDebugger::Task> VoxelGridGPUDebugger::create_generation_tasks(
    const Camera& camera,
    bool default_tasks_activity_state)
{
    LOG_METHOD();

    std::vector<Task> tasks = {
        Task{
            "ensure_free_chunks_gpu()" ,
            default_tasks_activity_state,
            [&](VulkanCommandBuffer& command_buffer){
                m_voxel_grid->ensure_free_chunks_gpu(
                    command_buffer,
                    camera.position,
                    math_utils::BITS,
                    math_utils::OFFSET
                );
            }
        },
        Task{
            "reset_load_list_counter()",
            default_tasks_activity_state,
            [&](VulkanCommandBuffer& command_buffer){
                m_voxel_grid->reset_load_list_counter(command_buffer);
            }
        },
        Task{
            "mark_chunk_to_generate()",
            default_tasks_activity_state,
            [&](VulkanCommandBuffer& command_buffer){
                m_voxel_grid->mark_chunk_to_generate(
                    command_buffer,
                    camera.position,
                    m_voxel_grid->m_params.generation_distance
                );
            }
        },
        Task{
            "merge_voxel_write_lists()",
            default_tasks_activity_state,
            [&](VulkanCommandBuffer& command_buffer){
                m_voxel_grid->merge_voxel_write_lists(
                    command_buffer,
                    m_voxel_grid->m_buffers.local_voxel_write_list,
                    m_voxel_grid->m_buffers.voxel_write_list
                );
            }
        },
        Task{
            "reset_voxel_write_list_counter()",
            default_tasks_activity_state,
            [&](VulkanCommandBuffer& command_buffer){
                m_voxel_grid->reset_voxel_write_list_counter(command_buffer, m_voxel_grid->m_buffers.local_voxel_write_list);
            }
        },
        Task{
            "mark_write_chunks_to_generate()",
            default_tasks_activity_state,
            [&](VulkanCommandBuffer& command_buffer){
                m_voxel_grid->m_shader_helper.prepare_dispatch_args(
                    command_buffer,
                    m_voxel_grid->m_buffers.dispatch_args,
                    BufferDispatchArg(&m_voxel_grid->m_buffers.voxel_write_list, 0)
                );
                m_voxel_grid->mark_write_chunks_to_generate(command_buffer, m_voxel_grid->m_buffers.dispatch_args);
            }
        },
        Task{
            "generate_terrain()",
            default_tasks_activity_state,
            [&](VulkanCommandBuffer& command_buffer){
                m_voxel_grid->m_shader_helper.prepare_dispatch_args(
                    command_buffer,
                    m_voxel_grid->m_buffers.dispatch_args, 
                    ValueDispatchArg(m_voxel_grid->vox_per_chunk()),
                    BufferDispatchArg(&m_voxel_grid->m_buffers.load_list, 0u)
                );
                m_voxel_grid->generate_terrain(command_buffer, m_voxel_grid->m_buffers.dispatch_args, 42);
            }
        },
        Task{
            "write_voxels_to_grid()",
            default_tasks_activity_state,
            [&](VulkanCommandBuffer& command_buffer){
                m_voxel_grid->m_shader_helper.prepare_dispatch_args(
                    command_buffer,
                    m_voxel_grid->m_buffers.dispatch_args,
                    BufferDispatchArg(&m_voxel_grid->m_buffers.voxel_write_list, 0u)
                );
                m_voxel_grid->write_voxels_to_grid(command_buffer, m_voxel_grid->m_buffers.dispatch_args);
            }
        },
        Task{
            "reset_voxel_write_list_counter()",
            default_tasks_activity_state,
            [&](VulkanCommandBuffer& command_buffer){
                m_voxel_grid->reset_voxel_write_list_counter(command_buffer, m_voxel_grid->m_buffers.voxel_write_list);
            }
        }
    };

    return tasks;
}
