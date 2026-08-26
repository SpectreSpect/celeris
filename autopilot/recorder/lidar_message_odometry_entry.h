#pragma once

#include "../../vulkan_self/logger/logger_header.h"
#include "../sensors/lidar/lidar_message.h"
#include "../odometry/odometry.h"

struct LidarMessageOdometryEntry {
public:
    LidarMessage lidar_message;
    Odometry odometry;
};