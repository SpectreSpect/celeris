#ifndef VOXEL_GRID_UTILS_GLSL
#define VOXEL_GRID_UTILS_GLSL

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

#endif // VOXEL_GRID_UTILS_GLSL
