#ifndef CHUNK_HASH_TABLE_INIT_CHUNK_META_GLSL
#define CHUNK_HASH_TABLE_INIT_CHUNK_META_GLSL

// Expected external symbols:
//   meta[]                 - SSBO array of ChunkMeta
//   NEED_GENERATION_FLAG_BIT
void init_chunk_meta(uint slot_id, uvec2 key, uint chunk_id) {
    meta[chunk_id].used = 1u;
    meta[chunk_id].key_lo = key.x;
    meta[chunk_id].key_hi = key.y;
    meta[chunk_id].dirty_flags = NEED_GENERATION_FLAG_BIT;
}

#endif // CHUNK_HASH_TABLE_INIT_CHUNK_META_GLSL
