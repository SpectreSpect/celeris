#ifndef VOXEL_GRID_UTILS_GLSL
#define VOXEL_GRID_UTILS_GLSL

#define CAT_(A, B) A##B
#define CAT(A, B) CAT_(A, B)
#define CAT3_(A, B, C) A##B##C
#define CAT3(A, B, C) CAT3_(A, B, C)
#define PR(PREFIX, NAME) CAT3(PREFIX, _, NAME)

uint div_up_u32(uint a, uint b) {
    return (a + b - 1u) / b;
}

uint ceil_log2_u32(uint x) {
    if (x <= 1u) {
        return 0u;
    }
    return uint(findMSB(x - 1u) + 1);
}

int floor_div(int a, uint d) {
    int divisor = int(d);
    int q = a / divisor;
    int r = a - q * divisor;
    if (r != 0 && a < 0) {
        q -= 1;
    }
    return q;
}

int floor_mod(int a, uint d) {
    int q = floor_div(a, d);
    return a - q * int(d);
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
    return hash_u32(h);
}

float hash_ivec2(ivec2 p, uint seed) {
    uint h = uint(p.x) * 374761393u
           + uint(p.y) * 668265263u
           + seed * 2246822519u;

    h = hash_u32(h);
    return float(h) * (1.0 / 4294967295.0);
}

uvec2 pack_key_uvec2(ivec3 c, int pack_offset, uint pack_bits) {
    uint b = pack_bits;
    uint m = mask_bits(b);

    uint ux = uint(c.x + pack_offset) & m;
    uint uy = uint(c.y + pack_offset) & m;
    uint uz = uint(c.z + pack_offset) & m;

    uint lo = uz;
    uint hi = 0u;

    if (b < 32u) {
        lo |= uy << b;
        if (b != 0u && b + b > 32u) {
            hi |= uy >> (32u - b);
        }
    } else {
        hi |= uy << (b - 32u);
    }

    uint shift = 2u * b;
    if (shift < 32u) {
        lo |= ux << shift;
        if (b != 0u && shift + b > 32u) {
            hi |= ux >> (32u - shift);
        }
    } else if (shift == 32u) {
        hi |= ux;
    } else {
        hi |= ux << (shift - 32u);
    }

    return uvec2(lo, hi);
}

uint extract_bits_uvec2(uvec2 k, uint shift, uint bits) {
    uint m = mask_bits(bits);
    if (bits == 0u) {
        return 0u;
    }

    if (shift < 32u) {
        uint lo_part = k.x >> shift;
        uint result = lo_part;
        uint remaining = 32u - shift;
        if (remaining < bits) {
            result |= k.y << remaining;
        }
        return result & m;
    }

    uint s = shift - 32u;
    return (k.y >> s) & m;
}

ivec3 unpack_key_to_coord(uvec2 key, int pack_offset, uint pack_bits) {
    uint bits = pack_bits;
    uint x = extract_bits_uvec2(key, 2u * bits, bits);
    uint y = extract_bits_uvec2(key, bits, bits);
    uint z = extract_bits_uvec2(key, 0u, bits);

    return ivec3(int(x) - pack_offset, int(y) - pack_offset, int(z) - pack_offset);
}

uint pack_color(vec4 rgba) {
    rgba = clamp(rgba, 0.0, 1.0);
    uint r = uint(rgba.r * 255.0 + 0.5);
    uint g = uint(rgba.g * 255.0 + 0.5);
    uint b = uint(rgba.b * 255.0 + 0.5);
    uint a = uint(rgba.a * 255.0 + 0.5);
    return (r << 24u) | (g << 16u) | (b << 8u) | a;
}

uint pack_color(vec3 rgb) {
    return pack_color(vec4(rgb, 1.0));
}

#endif // VOXEL_GRID_UTILS_GLSL
