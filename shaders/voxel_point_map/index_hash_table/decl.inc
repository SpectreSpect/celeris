#ifndef INDEX_HASH_TABLE_DECL
#define INDEX_HASH_TABLE_DECL

struct IndexHashTableSlot {
    uvec2 key;
    uint value;
    uint state;
};

#endif
