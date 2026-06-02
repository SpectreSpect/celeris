#ifndef CHUNK_HASH_TABLE_DECL_GLSL
#define CHUNK_HASH_TABLE_DECL_GLSL

// std430-friendly: uvec2 + uint + uint = 16 bytes.
// The generic hash-table implementation expects .state, .key and .value fields.
struct ChunkHashTableSlot {
    uvec2 key;
    uint value;
    uint state;
};

#endif // CHUNK_HASH_TABLE_DECL_GLSL
