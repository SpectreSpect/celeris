#ifndef ALLOCATOR_COMMON
#define ALLOCATOR_COMMON

// ---- state packing ----
#define ST_MASK_BITS 4u
#define ST_FREE 0u
#define ST_ALLOC 1u
#define ST_MERGED 2u
#define ST_WAIT_TO_FREE 4u
const uint ST_MASK = (1u << ST_MASK_BITS) - 1u;

#define ZERO_ATTEMPTS 32u

// ---- head state packing ----
#define HEAD_TAG_BITS 4u // 4 бита вроде мало, но пока ABA вроде нет
const uint HEAD_TAG_MASK = (1u << HEAD_TAG_BITS) - 1u;
const uint INVALID_HEAD_IDX = INVALID_ID >> HEAD_TAG_BITS;

uint pack_state(uint order, uint kind) { return (order << ST_MASK_BITS) | (kind & ST_MASK); }
uint pack_head(uint start, uint tag) {return (start << HEAD_TAG_BITS) | (tag & HEAD_TAG_MASK); }

#endif
