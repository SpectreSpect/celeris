#ifndef VOXEL_GRID_CHUNK_POOL_MARK_DIRTY_GLSL
#define VOXEL_GRID_CHUNK_POOL_MARK_DIRTY_GLSL

#include "../chunk_hash_table/lookup_remove.inc"

void mark_dirty(uint chunk_id) {
    uint was = atomicCompSwap(enqueued[chunk_id], 0u, 1u);
    if (was == 0u) {
        atomicOr(meta[chunk_id].dirty_flags, DIRTY_FLAG_BIT);
        uint dirty_id = atomicAdd(dirty_count, 1u);
        dirty_list[dirty_id] = chunk_id;
    }
}

void try_mark_neighbor(ivec3 chunk_coord) {
    uint slot_id = chunk_hash_table_lookup_hash_table_slot_id(
        pack_key_uvec2(chunk_coord, u_pack_offset, u_pack_bits),
        false
    );

    if (slot_id == INVALID_ID) {
        return;
    }

    uint chunk_id = chunk_hash_table_slots[slot_id].value;
    uint used = atomicAdd(meta[chunk_id].used, 0u);
    if (used == 1u) {
        mark_dirty(chunk_id);
    }
}

void mark_dirty_around(uint chunk_id, ivec3 chunk_coord) {
    mark_dirty(chunk_id);

    try_mark_neighbor(chunk_coord + ivec3( 1,  0,  0));
    try_mark_neighbor(chunk_coord + ivec3(-1,  0,  0));
    try_mark_neighbor(chunk_coord + ivec3( 0,  1,  0));
    try_mark_neighbor(chunk_coord + ivec3( 0, -1,  0));
    try_mark_neighbor(chunk_coord + ivec3( 0,  0,  1));
    try_mark_neighbor(chunk_coord + ivec3( 0,  0, -1));
}

#endif // VOXEL_GRID_CHUNK_POOL_MARK_DIRTY_GLSL
