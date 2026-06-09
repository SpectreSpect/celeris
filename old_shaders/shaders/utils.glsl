#pragma once

#define CAT_(A, B) A##B
#define CAT(A, B)  CAT_(A, B)
#define CAT3_(A, B, C) A##B##C
#define CAT3(A, B, C)  CAT3_(A, B, C)
#define PR(PREFIX, NAME) CAT3(PREFIX, _, NAME)
#define SF(NAME, SUFFIX) CAT3(NAME, _, SUFFIX)

#define INVALID_ID 0xFFFFFFFFu
#define BYTE_MASK 0xFFu

// uint max(uint a, uint b) {
//     return a > b ? a : b;
// }

uint div_up_u32(uint a, uint b) { 
    return (a + b - 1u) / b; 
}

uint ceil_log2_u32(uint x) {
    if (x <= 1u) return 0u;
    return uint(findMSB(x - 1u) + 1);
}

int floor_div(int a, int d) {
    int q = a / d;
    int r = a - q * d;
    if (r != 0 && a < 0) q -= 1;
    return q;
}

ivec3 floor_div(ivec3 a, ivec3 d) {
    return ivec3(
        floor_div(a.x, d.x),
        floor_div(a.y, d.y),
        floor_div(a.z, d.z)
    );
}

int floor_mod(int a, int d) {
    int q = floor_div(a, d);
    return a - q * d; // [0..d-1]
}

uint mask_bits(uint bits) { 
    return (bits >= 32u) ? 0xFFFFFFFFu : ((1u << bits) - 1u); 
}

uint hash_u32(uint x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

uint hash_uvec2(uvec2 v) {
    uint x = v.x * 1664525u + 1013904223u;
    uint y = v.y * 22695477u + 1u;
    uint h = x ^ (y + (x << 16) + (x >> 16));
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

float hash_ivec2(ivec2 p, uint seed) {
    uint h = uint(p.x) * 374761393u
           + uint(p.y) * 668265263u
           + seed     * 2246822519u;

    h = hash_u32(h);
    return float(h) * (1.0 / 4294967295.0);
}

uvec2 pack_key_uvec2(ivec3 c, uint pack_offset, uint pack_bits) {
    uint B = pack_bits;
    uint m = mask_bits(B);

    uint ux = uint(c.x + pack_offset) & m;
    uint uy = uint(c.y + pack_offset) & m;
    uint uz = uint(c.z + pack_offset) & m;

    uint lo = 0u, hi = 0u;
    lo |= uz;

    // uy at shift B
    if (B < 32u) {
        lo |= (uy << B);
        if (B != 0u && (B + B) > 32u) hi |= (uy >> (32u - B));
    } else {
        hi |= (uy << (B - 32u));
    }

    // ux at shift 2B
    uint s = 2u * B;
    if (s < 32u) {
        lo |= (ux << s);
        if (B != 0u && (s + B) > 32u) hi |= (ux >> (32u - s));
    } else if (s == 32u) {
        hi |= ux;
    } else {
        hi |= (ux << (s - 32u));
    }

    return uvec2(lo, hi);
}

uint extract_bits_uvec2(uvec2 k, uint shift, uint bits) {
    uint m = mask_bits(bits);
    if (bits == 0u) return 0u;

    if (shift < 32u) {
        uint lo_part = k.x >> shift;
        uint res = lo_part;
        uint rem = 32u - shift;
        if (rem < bits) res |= (k.y << rem);
        return res & m;
    } else {
        uint s = shift - 32u;
        return (k.y >> s) & m;
    }
}

ivec3 unpack_key_to_coord(uvec2 key2, uint pack_offset, uint pack_bits) {
    uint B = pack_bits;
    uint ux = extract_bits_uvec2(key2, 2u * B, B);
    uint uy = extract_bits_uvec2(key2, 1u * B, B);
    uint uz = extract_bits_uvec2(key2, 0u * B, B);

    return ivec3(int(ux) - pack_offset,
                 int(uy) - pack_offset,
                 int(uz) - pack_offset);
}

uint pack_color(vec4 rgba) {
    rgba = clamp(rgba, 0.0, 1.0);
    uint r = uint(rgba.r * 255.0 + 0.5);
    uint g = uint(rgba.g * 255.0 + 0.5);
    uint b = uint(rgba.b * 255.0 + 0.5);
    uint a = uint(rgba.a * 255.0 + 0.5);
    return (r << 24) | (g<<16) | (b<<8) | a;
}

uint pack_color(vec3 rgb) {
    return pack_color(vec4(rgb, 1u));
}

uint pack_color(uvec4 rgba) {
    return ((rgba.r & BYTE_MASK) << 24u) | ((rgba.g & BYTE_MASK) << 16u) | ((rgba.b & BYTE_MASK) << 8u) | (rgba.a & BYTE_MASK);
}

uint pack_color(uvec3 rgb) {
    return pack_color(uvec4(rgb, 0xFFu));
}
