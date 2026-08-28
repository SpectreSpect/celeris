#pragma once

// #include "../lidar_message.h"

#include "../../../../vulkan_self/logger/logger_header.h"

// ../../../..

class OdometryEstimator;
class LidarMessage;

class LidarScanDeskewer {
public:
    _XCLASS_NAME(LidarScanDeskewer);

    LidarScanDeskewer(OdometryEstimator& odometry_estimator);

    void deskew(LidarMessage& lidar_msg);

private:
    OdometryEstimator* m_odometry_estimator = nullptr;
};