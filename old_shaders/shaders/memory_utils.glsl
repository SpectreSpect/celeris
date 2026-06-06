#include "utils.glsl"

#ifdef M_SUFFIX
#ifdef BUFFER_NAME

uint SF(read_byte, M_SUFFIX)(uint offset_bytes) {
    uint src_uint_index = offset_bytes >> 2u;
    uint src_byte_index = offset_bytes & 3u;
    uint src_byte_shift = src_byte_index * 8u;

    return (CAT(BUFFER_NAME, [src_uint_index]) >> src_byte_shift) & BYTE_MASK;
}

uint SF(read_uint, M_SUFFIX)(uint offset_bytes) {
    uint byte_0 = SF(read_byte, M_SUFFIX)(offset_bytes + 0u);
    uint byte_1 = SF(read_byte, M_SUFFIX)(offset_bytes + 1u);
    uint byte_2 = SF(read_byte, M_SUFFIX)(offset_bytes + 2u);
    uint byte_3 = SF(read_byte, M_SUFFIX)(offset_bytes + 3u);
    return (byte_3 << 24u) | (byte_2 << 16u) | (byte_1 << 8u) | byte_0;
}

#undef M_SUFFIX
#undef BUFFER_NAME
#endif
#endif