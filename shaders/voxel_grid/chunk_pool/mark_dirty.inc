#ifndef MARK_DIRTY
#define MARK_DIRTY

#include "../../common/utils.inc"
#include "../chunk_hash_table/lookup_remove.inc"

bool mark_dirty_flag(uint chunk_id, uint dirty_flag) {
    uint was = atomicOr(enqueued[chunk_id], dirty_flag);
    if ((was & dirty_flag) == 0u) {
        atomicOr(meta[chunk_id].dirty_flags, dirty_flag);
        return true;
    }
    return false;
}

void mark_dirty(uint chunk_id) {
    if (mark_dirty_flag(chunk_id, DIRTY_FLAG_BIT)) {
        uint dirty_idx = atomicAdd(dirty_count, 1u);
        dirty_list[dirty_idx] = chunk_id;
    }
}

void try_mark_neighbor(ivec3 ncoord, bool read_only) {
    uint slot_id = chunk_hash_table_lookup_hash_table_slot_id(pack_key_uvec2(ncoord, u_pack_offset, u_pack_bits), read_only);
    if (slot_id == INVALID_ID) return;

    uint chunk_id = chunk_hash_table_slots[slot_id].value;

    uint used = atomicAdd(meta[chunk_id].used, 0u);
    if (used == 1u) {
        mark_dirty(chunk_id);
    }
}

void try_mark_neighbor(ivec3 ncoord) {
    try_mark_neighbor(ncoord, false);
}

void mark_dirty_around(uint chunkId, ivec3 chunk_coord) {
    mark_dirty(chunkId); // Заставляем перестроить меш у себя

    // А также у всех чанков вокруг
    try_mark_neighbor(chunk_coord + ivec3( 1, 0, 0));
    try_mark_neighbor(chunk_coord + ivec3(-1, 0, 0));
    try_mark_neighbor(chunk_coord + ivec3( 0, 1, 0));
    try_mark_neighbor(chunk_coord + ivec3( 0,-1, 0));
    try_mark_neighbor(chunk_coord + ivec3( 0, 0, 1));
    try_mark_neighbor(chunk_coord + ivec3( 0, 0,-1));
}

#endif
