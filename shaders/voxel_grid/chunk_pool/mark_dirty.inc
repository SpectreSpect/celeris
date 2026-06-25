#ifndef MARK_DIRTY
#define MARK_DIRTY

#include "../../common/utils.inc"
#include "../chunk_hash_table/lookup_remove.inc"

bool mark_dirty_flags(uint chunk_id, uint dirty_flags) {
    if (dirty_flags == 0u) return false;

    uint was = atomicOr(enqueued[chunk_id], dirty_flags);
    uint new_dirty_flags = dirty_flags & ~was;
    if (new_dirty_flags == 0u) return false;

    atomicOr(meta[chunk_id].dirty_flags, dirty_flags);
    return true;
}

void mark_dirty(uint chunk_id, uint dirty_flags) {
    if (mark_dirty_flags(chunk_id, dirty_flags)) {
        uint dirty_idx = atomicAdd(dirty_count, 1u);
        dirty_list[dirty_idx] = chunk_id;
    }
}

void mark_dirty_mesh(uint chunk_id) {
    mark_dirty(chunk_id, DIRTY_MESH_FLAG_BIT);
}

void try_mark_neighbor(ivec3 ncoord, bool read_only, uint dirty_flags) {
    uint slot_id = chunk_hash_table_lookup_hash_table_slot_id(pack_key_uvec2(ncoord, u_pack_offset, u_pack_bits), read_only);
    if (slot_id == INVALID_ID) return;

    uint chunk_id = chunk_hash_table_slots[slot_id].value;

    uint used = atomicAdd(meta[chunk_id].used, 0u);
    if (used == 1u) {
        mark_dirty(chunk_id, dirty_flags);
    }
}

void try_mark_dirty_mesh_neighbor(ivec3 ncoord, bool read_only) {
    try_mark_neighbor(ncoord, read_only, DIRTY_MESH_FLAG_BIT);
}

void try_mark_neighbor(ivec3 ncoord, uint dirty_flags) {
    try_mark_neighbor(ncoord, false, dirty_flags);
}

void try_mark_dirty_mesh_neighbor(ivec3 ncoord) {
    try_mark_dirty_mesh_neighbor(ncoord, false);
}

void mark_dirty_around(uint chunkId, ivec3 chunk_coord, uint dirty_flags) {
    mark_dirty(chunkId, dirty_flags); // Заставляем перестроить меш у себя

    // А также у всех чанков вокруг
    try_mark_neighbor(chunk_coord + ivec3( 1, 0, 0), dirty_flags);
    try_mark_neighbor(chunk_coord + ivec3(-1, 0, 0), dirty_flags);
    try_mark_neighbor(chunk_coord + ivec3( 0, 1, 0), dirty_flags);
    try_mark_neighbor(chunk_coord + ivec3( 0,-1, 0), dirty_flags);
    try_mark_neighbor(chunk_coord + ivec3( 0, 0, 1), dirty_flags);
    try_mark_neighbor(chunk_coord + ivec3( 0, 0,-1), dirty_flags);
}

void mark_dirty_mesh_around(uint chunkId, ivec3 chunk_coord) {
    mark_dirty_around(chunkId, chunk_coord, DIRTY_MESH_FLAG_BIT);
}

#endif
