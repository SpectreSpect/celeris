#include "lidar_scan_deskewer.h"

#include "../../../odometry/odometry_estimator.h"
#include "../lidar_message.h"

LidarScanDeskewer::LidarScanDeskewer(OdometryEstimator& odometry_estimator) 
    :   m_odometry_estimator(&odometry_estimator) {}


void LidarScanDeskewer::deskew(LidarMessage& lidar_msg) {
    LOG_METHOD();

    logger().check(m_odometry_estimator, "Odometry estimator was null");

    for (int i = 0; i < lidar_msg.points.size(); i++) {
        lidar_msg.points[i].position.x *= 10;
    }
}