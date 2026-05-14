#pragma once
#include <cstdint>

namespace Utils {
    constexpr uint32_t align_up(uint32_t value, uint32_t alignment) {
        return (value + alignment - 1) / alignment * alignment;
    }
};
