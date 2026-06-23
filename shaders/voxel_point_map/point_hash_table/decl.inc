#ifndef POINT_HASH_TABLE_DECL
#define POINT_HASH_TABLE_DECL

struct PointInstance {
    vec4 position;
    vec4 color;
};

struct PointHashTableSlot {
    uvec2 key;
    PointInstance value;
    uint state;
};

#endif
