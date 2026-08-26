#pragma once

#include <filesystem>
#include <fstream>
#include <cstdint>

#include "../../vulkan_self/logger/logger_header.h"
#include "../sensors/lidar/lidar_scan.h"
#include "../odometry/odometry.h"


class LidarOdometryRecorder {
public:
    _XCLASS_NAME(LidarOdometryRecorder);

    ~LidarOdometryRecorder() noexcept;

    void start(std::filesystem::path path);
    void stop();
    void record(LidarScan& lidar_scan, const Odometry& odometry);
    bool is_recording() const noexcept;

private:
    bool m_is_recording = false;
    std::filesystem::path m_recording_dir;
    std::ofstream m_index_file;
    uint32_t m_record_count = 0;
};