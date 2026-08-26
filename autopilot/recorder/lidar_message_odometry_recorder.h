#pragma once

#include <filesystem>
#include <fstream>
#include <cstdint>

#include "../../vulkan_self/logger/logger_header.h"
#include "../sensors/lidar/lidar_message.h"
#include "../odometry/odometry.h"

class LidarMessageOdometryRecorder {
public:
    _XCLASS_NAME(LidarMessageOdometryRecorder);

    ~LidarMessageOdometryRecorder() noexcept;

    void start(std::filesystem::path path);
    void stop();
    void record(const LidarMessage& lidar_msg, const Odometry& odometry);
    bool is_recording() const noexcept;

private:
    bool m_is_recording = false;
    std::filesystem::path m_recording_dir;
    std::ofstream m_index_file;
    uint32_t m_record_count = 0;
};