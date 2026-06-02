#ifndef CHUNK_HASH_TABLE_CHUNK_KEY_COMP_GLSL
#define CHUNK_HASH_TABLE_CHUNK_KEY_COMP_GLSL

// uvec2 cannot be compared with == as a scalar bool in portable GLSL.
bool chunk_key_comp(uvec2 a, uvec2 b) {
    return all(equal(a, b));
}

#endif // CHUNK_HASH_TABLE_CHUNK_KEY_COMP_GLSL
