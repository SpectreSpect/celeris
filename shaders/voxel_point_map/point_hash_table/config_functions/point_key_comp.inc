#ifndef POINT_HASH_TABLE_POINT_KEY_COMP
#define POINT_HASH_TABLE_POINT_KEY_COMP

bool point_key_comp(uvec2 a, uvec2 b) {
    return all(equal(a, b));
}

#endif
