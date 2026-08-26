#pragma once

#include <filesystem>
#include <fstream>
#include <cstdint>
#include <vector>

#include "../../../renderer/point_cloud/point_instance.h"
#include "../../../vulkan_self/logger/logger_header.h"

class LidarMessage {
public:
    _XCLASS_NAME(LidarMessage);

    std::vector<PointInstance> points;
    std::vector<uint64_t> timestamps;
    uint32_t latest_point_id = -1;

    LidarMessage() = default;
    LidarMessage(std::filesystem::path path);

    void save(std::filesystem::path path);
    static LidarMessage load(std::filesystem::path path);
};