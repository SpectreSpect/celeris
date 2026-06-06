#version 430
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(std430, binding = 0) readonly buffer PrefabBuffer {
    uint prefab_data[];
};

layout(std430, binding = 1) buffer ClearableBuffer {
    uint clearable_buffer_data[];
};

#define BYTE_MASK 0xFFu

uniform uint prefab_data_bytes;
uniform uint clearable_data_bytes;

uniform uint clearable_data_offset_bytes;

uniform uint count_invocations_x;
uniform uint count_invocations_y;
uniform uint count_invocations_z;

uniform uint invocation_stride_bytes;

// ------------------------------------------------------------
// Чтение одного байта из бесконечно повторяющегося prefab.
// byte_offset задаётся относительно НАЧАЛА очищаемой области,
// а не относительно начала destination buffer.
// ------------------------------------------------------------
uint read_prefab_byte(uint byte_offset) {
    uint src_byte_offset = byte_offset % prefab_data_bytes;
    uint src_uint_index  = src_byte_offset >> 2u; // / 4
    uint src_byte_index  = src_byte_offset & 3u;  // % 4
    uint src_shift       = src_byte_index * 8u;

    return (prefab_data[src_uint_index] >> src_shift) & BYTE_MASK;
}

// ------------------------------------------------------------
// Сборка одного uint из 4 последовательных байтов prefab,
// начиная с byte_offset (относительно начала очищаемой области).
// ------------------------------------------------------------
uint build_prefab_uint(uint byte_offset) {
    uint b0 = read_prefab_byte(byte_offset + 0u);
    uint b1 = read_prefab_byte(byte_offset + 1u);
    uint b2 = read_prefab_byte(byte_offset + 2u);
    uint b3 = read_prefab_byte(byte_offset + 3u);

    return (b0 << 0u) | (b1 << 8u) | (b2 << 16u) | (b3 << 24u);
}

// ------------------------------------------------------------
// Атомарная запись одного байта в destination buffer.
// dst_byte_offset задаётся в абсолютных байтах относительно
// начала clearable_buffer_data.
// ------------------------------------------------------------
void write_dst_byte_atomic(uint dst_byte_offset, uint byte_value) {
    uint dst_uint_index = dst_byte_offset >> 2u; // / 4
    uint dst_byte_index = dst_byte_offset & 3u;  // % 4
    uint dst_shift      = dst_byte_index * 8u;

    uint clear_mask  = ~(BYTE_MASK << dst_shift);
    uint insert_bits = (byte_value & BYTE_MASK) << dst_shift;

    for (;;) {
        uint oldv = atomicAdd(clearable_buffer_data[dst_uint_index], 0u);
        uint newv = (oldv & clear_mask) | insert_bits;

        if (atomicCompSwap(clearable_buffer_data[dst_uint_index], oldv, newv) == oldv) {
            break;
        }
    }
}

void main() {
    if (prefab_data_bytes == 0u || invocation_stride_bytes == 0u || clearable_data_bytes == 0u) {
        return;
    }

    uvec3 gid = gl_GlobalInvocationID.xyz;
    if (gid.x >= count_invocations_x ||
        gid.y >= count_invocations_y ||
        gid.z >= count_invocations_z) {
        return;
    }

    uint invocation_idx =
          gid.x
        + gid.y * count_invocations_x
        + gid.z * (count_invocations_x * count_invocations_y);

    // Относительный диапазон байт, который должен обработать invocation
    // внутри очищаемой области: [rel_start, rel_end)
    uint rel_start = invocation_idx * invocation_stride_bytes;

    // Если invocation полностью за пределами очищаемой области — выходим
    if (rel_start >= clearable_data_bytes) {
        return;
    }

    // Клип без риска underflow/overflow
    uint remaining = clearable_data_bytes - rel_start;
    uint rel_len   = min(invocation_stride_bytes, remaining);
    uint rel_end   = rel_start + rel_len;

    // Абсолютный диапазон байт внутри destination buffer: [abs_start, abs_end)
    uint abs_start = clearable_data_offset_bytes + rel_start;
    uint abs_end   = clearable_data_offset_bytes + rel_end;

    // Выровненная середина по destination
    uint aligned_begin = (abs_start + 3u) & ~3u; // ceil to multiple of 4
    uint aligned_end   = abs_end & ~3u;          // floor to multiple of 4

    if (aligned_begin > abs_end) {
        aligned_begin = abs_end;
    }
    if (aligned_end < aligned_begin) {
        aligned_end = aligned_begin;
    }

    // Быстрый путь для середины корректен только если:
    // 1) размер prefab кратен 4
    // 2) начало очищаемой области кратно 4
    // Тогда относительный source offset у aligned middle тоже кратен 4.
    bool can_fast_middle =
        ((prefab_data_bytes & 3u) == 0u) &&
        ((clearable_data_offset_bytes & 3u) == 0u);

    // ---------------- left unaligned tail ----------------
    for (uint abs_byte_off = abs_start; abs_byte_off < aligned_begin; ++abs_byte_off) {
        uint rel_byte_off = abs_byte_off - clearable_data_offset_bytes;
        uint v = read_prefab_byte(rel_byte_off);
        write_dst_byte_atomic(abs_byte_off, v);
    }

    // ---------------- aligned middle ----------------
    for (uint abs_byte_off = aligned_begin; abs_byte_off < aligned_end; abs_byte_off += 4u) {
        uint dst_uint_index = abs_byte_off >> 2u;
        uint rel_byte_off   = abs_byte_off - clearable_data_offset_bytes;

        if (can_fast_middle) {
            uint src_byte_offset = rel_byte_off % prefab_data_bytes;
            uint src_uint_index  = src_byte_offset >> 2u;
            clearable_buffer_data[dst_uint_index] = prefab_data[src_uint_index];
        } else {
            clearable_buffer_data[dst_uint_index] = build_prefab_uint(rel_byte_off);
        }
    }

    // ---------------- right unaligned tail ----------------
    for (uint abs_byte_off = aligned_end; abs_byte_off < abs_end; ++abs_byte_off) {
        uint rel_byte_off = abs_byte_off - clearable_data_offset_bytes;
        uint v = read_prefab_byte(rel_byte_off);
        write_dst_byte_atomic(abs_byte_off, v);
    }
}