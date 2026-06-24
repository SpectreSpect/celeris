#ifndef INDEX_HASH_TABLE_INDEX_KEY_COMP
#define INDEX_HASH_TABLE_INDEX_KEY_COMP

bool index_key_comp(uvec2 a, uvec2 b) {
    return all(equal(a, b));
}

#endif
