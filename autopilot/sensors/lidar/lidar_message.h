#pragma once

#include <cstdint>
#include <vector>

#include "../../../renderer/point_cloud/point_instance.h"

class LidarMessage {
public:
    std::vector<PointInstance> points;
    std::vector<uint64_t> timestamps;
    uint32_t latest_point_id = -1;
};