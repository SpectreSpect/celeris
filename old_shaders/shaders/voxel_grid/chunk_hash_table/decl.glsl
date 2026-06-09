#pragma once

struct ChunkHashTableSlot {
    uvec2 key;
    uint value;
    uint state;
};
