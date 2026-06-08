#ifndef CHUNK_HASH_TABLE_CHUNK_KEY_COMP
#define CHUNK_HASH_TABLE_CHUNK_KEY_COMP

bool chunk_key_comp(uvec2 a, uvec2 b) {
    return all(equal(a, b));
}

#endif
