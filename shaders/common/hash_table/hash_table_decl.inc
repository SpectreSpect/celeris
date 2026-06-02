#ifndef HASH_TABLE_DECL_VULKAN_GLSL
#define HASH_TABLE_DECL_VULKAN_GLSL

// Vulkan GLSL include file. Do not put #version here.
// Include it from a real shader after:
//   #version 450
//   #extension GL_GOOGLE_include_directive : require

#ifndef INVALID_ID
#define INVALID_ID 0xFFFFFFFFu
#endif

#ifndef SLOT_EMPTY
#define SLOT_EMPTY    0xFFFFFFFFu
#endif

#ifndef SLOT_LOCKED
#define SLOT_LOCKED   0xFFFFFFFEu
#endif

#ifndef SLOT_TOMB
#define SLOT_TOMB     0xFFFFFFFDu
#endif

#ifndef SLOT_OCCUPIED
#define SLOT_OCCUPIED 0xFFFFFFFCu
#endif

#ifndef MAX_PROBES
#define MAX_PROBES 128u
#endif

#ifndef COUNT_TABLE_COUNTERS
#define COUNT_TABLE_COUNTERS 16u
#endif

// Macro helpers used by the template-like implementation files.
// HT_PREFIX should include the trailing underscore if you want one, e.g.:
//   #define HT_PREFIX chunk_
#ifndef HT_CAT_IMPL
#define HT_CAT_IMPL(A, B) A##B
#endif

#ifndef HT_CAT
#define HT_CAT(A, B) HT_CAT_IMPL(A, B)
#endif

#ifndef HT_CAT3
#define HT_CAT3(A, B, C) HT_CAT(HT_CAT(A, B), C)
#endif

struct HashTableCounters {
    uint count_empty[COUNT_TABLE_COUNTERS];
    uint count_occupied[COUNT_TABLE_COUNTERS];
    uint count_tomb[COUNT_TABLE_COUNTERS];
};

#endif // HASH_TABLE_DECL_VULKAN_GLSL
