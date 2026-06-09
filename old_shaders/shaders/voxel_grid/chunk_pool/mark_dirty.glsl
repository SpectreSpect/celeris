#pragma once
#include "../../utils.glsl"
#include "../chunk_hash_table/lookup_remove.glsl"

void mark_dirty(uint chunkId) {
    uint was = atomicCompSwap(enqueued[chunkId], 0u, 1u);
    if (was == 0u) {
        atomicOr(meta[chunkId].dirty_flags, DIRTY_FLAG_BIT);
        uint di = atomicAdd(dirty_count, 1u);
        dirty_list[di] = chunkId;
    }
}

void try_mark_neighbor(ivec3 ncoord, bool read_only = false) {
    uint slot_id = chunk_hash_table_lookup_hash_table_slot_id(pack_key_uvec2(ncoord, u_pack_offset, u_pack_bits), read_only);
    if (slot_id == INVALID_ID) return;

    uint chunk_id = chunk_hash_table_slots[slot_id].value;

    uint used = atomicAdd(meta[chunk_id].used, 0u);
    if (used == 1u) {
        mark_dirty(chunk_id);
    }
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
