#include "lidar_scan_deskewer.h"

#include "../../../odometry/odometry_estimator.h"
#include "../../../odometry/odometry.h"
#include "../lidar_message.h"

LidarScanDeskewer::LidarScanDeskewer(OdometryEstimator& odometry_estimator) 
    :   m_odometry_estimator(&odometry_estimator) {}


void LidarScanDeskewer::deskew(LidarMessage& lidar_msg, Odometry& scan_odometry) {
    LOG_METHOD();

    logger().check(m_odometry_estimator, "Odometry estimator was null");

    // Odometry scan_odometry = lidar_msg.

    for (int i = 0; i < lidar_msg.points.size(); i++) {
        PointInstance& point = lidar_msg.points[i];
        const uint32_t& timestamp = lidar_msg.timestamps[i];

        Odometry point_odometry;
        m_odometry_estimator->interpolate_odometry(timestamp, point_odometry);

        glm::vec3 position_offset = point_odometry.position - scan_odometry.position;

        // point.position.x *= 10;
        point.position += glm::vec4(position_offset, 0.0f);
    }
}