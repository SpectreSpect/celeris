#pragma once

#include <filesystem>
#include <vector>

#include "../../vulkan_self/logger/logger_header.h"
#include "lidar_message_odometry_entry.h"


class LidarMessageOdometryRecordering {
public:
    _XCLASS_NAME(LidarMessageOdometryRecordering);

    LidarMessageOdometryRecordering() = default;
    LidarMessageOdometryRecordering(
        std::filesystem::path path, 
        int first_entry_id = -1,
        int last_entry_id = -1
    );

    void load(
        std::filesystem::path path, 
        int first_entry_id = -1,
        int last_entry_id = -1
    );

    LidarMessageOdometryEntry& get_entry(size_t index);
    uint32_t size() const noexcept;

private:
    std::vector<LidarMessageOdometryEntry> m_entries;
};