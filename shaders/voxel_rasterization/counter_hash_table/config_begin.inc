#define HT_PREFIX counter_hash_table
#define SLOT_VALUE_TYPE CounterAllocMeta
#define SLOT_KEY_TYPE uvec2
#define SLOT_TYPE CounterHashTableSlot
#define TABLE_COUNTERS counter_hash_table_counters
#define SLOTS_BUFFER counter_hash_table_slots
#define HASH_TABLE_SIZE u_counter_hash_table_size
#define KEY_HASH_FUNC hash_uvec2
#define KEY_COMP_FUNC chunk_key_comp
#define VALUE_INIT_FUNC counter_alloc_meta_init
// #define INIT_SLOT_CALLBACK init_chunk_meta
