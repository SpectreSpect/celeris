#ifndef CHUNK_HASH_TABLE_POP_FREE_CHUNK_ID_GLSL
#define CHUNK_HASH_TABLE_POP_FREE_CHUNK_ID_GLSL

// Expected external symbols:
//   free_count - SSBO uint lvalue
//   free_list[] - SSBO array of free chunk ids
//   INVALID_ID
uint pop_free_chunk_id(uvec2 key, out bool success) {
    for (;;) {
        uint old_counter = atomicAdd(free_count, 0u);
        if (old_counter == 0u) {
            success = false;
            return INVALID_ID;
        }

        if (atomicCompSwap(free_count, old_counter, old_counter - 1u) == old_counter) {
            memoryBarrierBuffer();
            success = true;
            return free_list[old_counter - 1u];
        }
    }
}

#endif // CHUNK_HASH_TABLE_POP_FREE_CHUNK_ID_GLSL
