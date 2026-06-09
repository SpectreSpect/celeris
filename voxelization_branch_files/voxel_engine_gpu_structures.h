#pragma once
#define NOMINMAX
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

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

static constexpr uint32_t DIRTY_FLAG_BIT = 1u;
static constexpr uint32_t NEED_GENERATION_FLAG_BIT = 2u;

static constexpr uint32_t VOXEL_TYPE_BITS = 16u;
static constexpr uint32_t VOXEL_TYPE_MASK = (1u << VOXEL_TYPE_BITS) - 1u;

static constexpr uint32_t VOXEL_VISABILITY_FLAG_BIT = 1u; // Определяет, видим ли воксель
static constexpr uint32_t VOXEL_EASY_OVERWRITE_FLAG_BIT = 2u; // Определяет, можно ли заменять воксель как будто бы он "воздух" или "вода" в майне

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
