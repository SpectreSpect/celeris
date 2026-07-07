#pragma once

#include <cstdint>
#include <glm/glm.hpp>

static constexpr uint32_t INVALID_ID = 0xFFFFFFFFu;

static constexpr uint32_t ST_MASK_BITS = 4u;
static constexpr uint32_t ST_MASK = (1u << ST_MASK_BITS) - 1u;

static constexpr uint32_t ST_FREE = 0u;
static constexpr uint32_t ST_ALLOC = 1u;
static constexpr uint32_t ST_MERGED = 2u;

static constexpr uint32_t HEAD_TAG_BITS = 4u;
static constexpr uint32_t HEAD_TAG_MASK = (1u << HEAD_TAG_BITS) - 1u;
static constexpr uint32_t INVALID_HEAD_IDX = INVALID_ID >> HEAD_TAG_BITS;

static constexpr uint32_t SLOT_EMPTY = 0xFFFFFFFFu;
static constexpr uint32_t SLOT_LOCKED = 0xFFFFFFFEu;
static constexpr uint32_t SLOT_TOMB = 0xFFFFFFFDu; 
static constexpr uint32_t SLOT_OCCUPIED = 0xFFFFFFFCu;

static constexpr uint32_t OVERWRITE_BIT = 1u;

static constexpr uint32_t DIRTY_MESH_FLAG_BIT = 1u;
static constexpr uint32_t NEED_GENERATION_FLAG_BIT = 2u;
static constexpr uint32_t DIRTY_INFLATE_FLAG_BIT = 4u;
static constexpr uint32_t ENQUEUED_FAILED_DIRTY_MESH_FLAG_BIT = 8u;

static constexpr uint32_t VOXEL_TYPE_BITS = 16u;
static constexpr uint32_t VOXEL_TYPE_MASK = (1u << VOXEL_TYPE_BITS) - 1u;

static constexpr uint32_t VOXEL_VISABILITY_FLAG_BIT = 1u; // Определяет, видим ли воксель
static constexpr uint32_t VOXEL_EASY_OVERWRITE_FLAG_BIT = 2u; // Определяет, можно ли заменять воксель как будто бы он "воздух" или "вода" в майне
static constexpr uint32_t VOXEL_INFLATED_BIT = 4u; // Определяет, пересекает ли воксель раздутые видимые воксели
static constexpr uint32_t VOXEL_CURVATURE_LIMIT_EXCEEDED_BIT = 8u; // Определяет, превышает ли воксель лимит кривизны
static constexpr uint32_t VOXEL_RECENTLY_INSERTED_BIT = 16u; // Определяет, был ли воксель вставлен в текущем батче записи

struct alignas(8) VoxelDataGPU {
    uint32_t type_flags;
    uint32_t color;

    VoxelDataGPU() = default;

    inline VoxelDataGPU(uint32_t type, uint32_t flags, uint32_t color) {
        init(type, flags, color);
    }

    inline VoxelDataGPU(uint32_t type, uint32_t flags, glm::ivec4 color) {
        uint32_t packed_color = ((color.x & 0xFFu) << 24u) | ((color.y & 0xFFu) << 16u) | ((color.z & 0xFFu) << 8u) | (color.w & 0xFFu);
        init(type, flags, packed_color);
    }

    inline VoxelDataGPU(uint32_t type, uint32_t flags, glm::ivec3 color) {
        uint32_t packed_color = ((color.x & 0xFFu) << 24u) | ((color.y & 0xFFu) << 16u) | ((color.z & 0xFFu) << 8u) | 0xFFu;
        init(type, flags, packed_color);
    }

    inline void init(uint32_t type, uint32_t flags, uint32_t color) {
        this->type_flags = (flags << VOXEL_TYPE_BITS) | (type & VOXEL_TYPE_MASK);
        this->color = color;
    }

    inline glm::vec4 color_vec4() const {
        return glm::vec4(
            static_cast<float>((color >> 24u) & 0xFFu),
            static_cast<float>((color >> 16u) & 0xFFu),
            static_cast<float>((color >> 8u) & 0xFFu),
            static_cast<float>(color & 0xFFu)
        ) / 255.0f;
    }

    inline bool is_solid() const {
        return ((type_flags >> VOXEL_TYPE_BITS) & VOXEL_VISABILITY_FLAG_BIT) > 0;
    }

    inline bool is_inflated() const {
        return ((type_flags >> VOXEL_TYPE_BITS) & VOXEL_INFLATED_BIT) > 0;
    }

    inline bool exceeds_curvature_limit() const {
        return ((type_flags >> VOXEL_TYPE_BITS) & VOXEL_CURVATURE_LIMIT_EXCEEDED_BIT) > 0;
    }

    inline bool was_recently_inserted() const {
        return ((type_flags >> VOXEL_TYPE_BITS) & VOXEL_RECENTLY_INSERTED_BIT) > 0;
    }
};

static_assert(sizeof(VoxelDataGPU) == 8);
static_assert(alignof(VoxelDataGPU) == 8);

struct ChunkHashTableSlot {
    alignas(8) uint64_t key;
    uint32_t value;
    uint32_t state;
};

static_assert(sizeof(ChunkHashTableSlot) == 16);
static_assert(alignof(ChunkHashTableSlot) == 8);

struct CounterAllocMeta {
    uint32_t count_triangles;
    uint32_t triangle_indices_base;
    uint32_t triangle_emmit_counter;
};

struct CounterHashTableSlot {
    alignas(8) uint64_t key;
    CounterAllocMeta value;
    uint32_t state;
};
static_assert(sizeof(CounterHashTableSlot) == 24);
static_assert(alignof(CounterHashTableSlot) == 8);


static constexpr uint32_t COUNT_TABLE_COUNTERS = 16u;

struct HashTableCounters {
    uint32_t count_empty[COUNT_TABLE_COUNTERS];
    uint32_t count_occupied[COUNT_TABLE_COUNTERS];
    uint32_t count_tomb[COUNT_TABLE_COUNTERS];

    uint32_t reduce_read_count(size_t counter_offset_bytes) {
        uint32_t* counter = (uint32_t*)((char*)this + counter_offset_bytes);
        uint32_t count = 0u;
        for (uint32_t i = 0u; i < COUNT_TABLE_COUNTERS; i++) count += counter[i];
        return count;
    }

    uint32_t reduce_read_count_empty() { return reduce_read_count(offsetof(HashTableCounters, count_empty)); }
    uint32_t reduce_read_count_occupied() { return reduce_read_count(offsetof(HashTableCounters, count_occupied)); }
    uint32_t reduce_read_count_tomb() { return reduce_read_count(offsetof(HashTableCounters, count_tomb)); }
};
static_assert(sizeof(HashTableCounters) == 192);


struct alignas(16) VoxelWriteGPU {
    glm::ivec4 world_voxel;  // xyz, w unused
    VoxelDataGPU voxel_data;
    uint32_t set_flags;
    uint32_t pad1;
};

static_assert(sizeof(VoxelWriteGPU) == 32);
static_assert(alignof(VoxelWriteGPU) == 16);

struct alignas(16) VoxelWriteUniqueSlotGPU {
    glm::ivec4 world_voxel;
    uint32_t selected_rank;
    uint32_t state;
    uint32_t pad0;
    uint32_t pad1;
};

static_assert(sizeof(VoxelWriteUniqueSlotGPU) == 32);
static_assert(alignof(VoxelWriteUniqueSlotGPU) == 16);

struct BucketHead {
    uint32_t id;
    uint32_t count;
};

struct ChunkMetaGPU {
    uint32_t used;
    uint32_t key_lo;
    uint32_t key_hi;
    uint32_t dirty_flags;
};
static_assert(sizeof(ChunkMetaGPU) == 16);

struct DrawElementsIndirectCommand {
    uint32_t count;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t  baseVertex;
    uint32_t baseInstance;
};
static_assert(sizeof(DrawElementsIndirectCommand) == 20);

struct alignas(16) VertexGPU {
    glm::vec4 pos;    // xyz position, w=1
    uint32_t color;  // RGBA8
    uint32_t face;   // 0..5
    uint32_t pad0;
    uint32_t pad1;
};
static_assert(sizeof(VertexGPU) == 32);
static_assert(alignof(VertexGPU) == 16);

struct ChunkMeshAlloc {
    uint32_t v_startPage; 
    uint32_t v_order; 
    uint32_t needV; 
    uint32_t i_startPage; 
    uint32_t i_order; 
    uint32_t needI;
    uint32_t is_valid;
};

struct AllocNode {
    uint32_t page;
    uint32_t next;
};

struct MeshPoolClearUniform {
    uint32_t u_vb_pages;
    uint32_t u_ib_pages;
    uint32_t u_vb_nodes;
    uint32_t u_ib_nodes;
    uint32_t u_vb_heads_count;
    uint32_t u_ib_heads_count;
    uint32_t u_max_chunks;
    uint32_t pad0;
};

struct MeshPoolSeedUniform {
    uint32_t u_vb_max_order;
    uint32_t u_ib_max_order;
    uint32_t pad1;
    uint32_t pad2;
};

struct alignas(16) BuildIndirectCmdsUniform {
    glm::ivec4 u_chunk_dim;
    glm::vec4  u_voxel_size;

    uint32_t u_max_chunks;
    uint32_t u_ib_page_size_bytes;
    uint32_t u_pack_bits;
    int32_t  u_pack_offset;

    float render_distance;
    uint32_t _pad0;
    uint32_t _pad1;
    uint32_t _pad2;
};

static_assert(sizeof(BuildIndirectCmdsUniform) == 64);
