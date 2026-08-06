#pragma once

#include <cstdint>

struct LidarMessageHeader {
    uint64_t timestamp_ns;
    uint32_t point_count;      
};