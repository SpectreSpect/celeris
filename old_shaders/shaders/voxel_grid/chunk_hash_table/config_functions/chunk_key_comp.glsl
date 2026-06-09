#pragma once

bool chunk_key_comp(uvec2 a, uvec2 b) {
    return all(equal(a, b));
}
