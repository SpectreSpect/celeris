#ifndef CHUNK_HASH_TABLE_DECL
#define CHUNK_HASH_TABLE_DECL

struct ChunkHashTableSlot {
    uvec2 key;
    uint value;
    uint state;
};

#endif
