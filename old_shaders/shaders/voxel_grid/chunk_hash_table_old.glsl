
#include "../utils.glsl"

#define SLOT_VALUE_TYPE uint // id чанка
#define SLOT_KEY_TYPE uvec2 // packed коодинаты чанка

#ifndef INCLUDED_CHUNK_HASH_TABLE_STRUCT
    #define INCLUDED_CHUNK_HASH_TABLE_STRUCT
    struct ChunkHashTableSlot {
        SLOT_VALUE_TYPE value;
        SLOT_KEY_TYPE key;
        uint state;
    };
#endif

#define COUNT_TOMBS chunk_hash_table_count_tombs
#define SLOTS_BUFFER chunk_hash_table_slots
#define KEY_HASH_FUNC hash_uvec2

#ifndef INCLUDED_CHUNK_KEY_COMP
    #define INCLUDED_CHUNK_KEY_COMP
    bool chunk_key_comp(SLOT_KEY_TYPE a, SLOT_KEY_TYPE b) {
        return all(equal(a, b));
    }
#endif
#define KEY_COMP_FUNC chunk_key_comp 

#if (defined(INCLUDE_ALL_HASH_TABLE) || defined(INCLUDE_HASH_TABLE_GET_OR_CREATE)) && !defined(NOT_INCLUDE_HASH_TABLE_GET_OR_CREATE)
    #ifndef INCLUDED_POP_FREE_CHUNK_ID
        #define INCLUDED_POP_FREE_CHUNK_ID
        SLOT_VALUE_TYPE pop_free_chunk_id(SLOT_KEY_TYPE key, out bool success) {
            for (;;) {
                uint old_counter = atomicAdd(free_count, 0u);
                if (old_counter == 0u) {
                    success = false;
                    return INVALID_ID;
                }
                
                if (atomicCompSwap(free_count, old_counter, old_counter - 1u) == old_counter) {
                    memoryBarrierBuffer();
                    success = true;
                    return free_list[old_counter - 1u];
                }
            }

            success = false;
            return INVALID_ID;
        }
    #endif
    #define VALUE_INIT_FUNC pop_free_chunk_id

    #ifndef INCLUDED_INIT_CHUNK_META
        #define INCLUDED_INIT_CHUNK_META
        void init_chunk_meta(uint slot_id, SLOT_KEY_TYPE key, SLOT_VALUE_TYPE chunk_id) {
            meta[chunk_id].used = 1u;
            meta[chunk_id].key_lo = key.x;
            meta[chunk_id].key_hi = key.y;
            meta[chunk_id].dirty_flags = NEED_GENERATION_FLAG_BIT;
        }
    #endif
    #define INIT_SLOT_CALLBACK init_chunk_meta
#endif

#define NOT_UNDEF
#define NOT_INCLUDE_ALL_HASH_TABLE

#ifndef CHUNK_HASH_TABLE_COMMON_INCLUDED
#define CHUNK_HASH_TABLE_COMMON_INCLUDED
#define INCLUDE_HASH_TABLE_COMMON
#include "../common/hash_table.glsl"
#undef INCLUDE_HASH_TABLE_COMMON
#endif

#ifndef CHUNK_HASH_TABLE_LOOKUP_REMOVE_INCLUDED
#define CHUNK_HASH_TABLE_LOOKUP_REMOVE_INCLUDED
#define INCLUDE_HASH_TABLE_LOOKUP_REMOVE
#include "../common/hash_table.glsl"
#undef INCLUDE_HASH_TABLE_LOOKUP_REMOVE
#endif

#ifndef CHUNK_HASH_TABLE_GET_OR_CREATE_INCLUDED
#define CHUNK_HASH_TABLE_GET_OR_CREATE_INCLUDED
#define INCLUDE_HASH_TABLE_GET_OR_CREATE
#include "../common/hash_table.glsl"
#undef INCLUDE_HASH_TABLE_GET_OR_CREATE
#endif
