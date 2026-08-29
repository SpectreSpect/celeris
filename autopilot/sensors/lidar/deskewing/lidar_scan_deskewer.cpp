#include "lidar_scan_deskewer.h"

#include "../../../odometry/odometry_estimator.h"
#include "../../../odometry/odometry.h"
#include "../lidar_message.h"

#include <algorithm>

LidarScanDeskewer::LidarScanDeskewer(OdometryEstimator& odometry_estimator) 
    :   m_odometry_estimator(&odometry_estimator) {}


void LidarScanDeskewer::deskew(LidarMessage& lidar_msg, Odometry& scan_odometry) {
    LOG_METHOD();

    logger().check(m_odometry_estimator, "Odometry estimator was null");

    if (m_odometry_estimator == nullptr)
        return;

    const glm::quat scan_orientation =
        glm::normalize(scan_odometry.orientation);
    const glm::quat world_to_scan = glm::inverse(scan_orientation);
    const std::size_t point_count =
        std::min(lidar_msg.points.size(), lidar_msg.timestamps.size());

    for (std::size_t i = 0; i < point_count; ++i) {
        PointInstance& point = lidar_msg.points[i];
        const uint64_t timestamp = lidar_msg.timestamps[i];

        Odometry point_odometry;
        if (!m_odometry_estimator->interpolate_odometry(
                timestamp, point_odometry)) {
            continue;
        }

        // Transform the measurement from the LiDAR frame at the point's
        // timestamp into the LiDAR frame at the scan reference timestamp:
        // p_scan = R_scan^-1 * (R_point * p + t_point - t_scan).
        const glm::quat point_to_scan = world_to_scan *
            glm::normalize(point_odometry.orientation);
        const glm::vec3 translation_in_scan = world_to_scan *
            (point_odometry.position - scan_odometry.position);

        const glm::vec3 deskewed_position =
            point_to_scan * glm::vec3(point.position) + translation_in_scan;
        point.position = glm::vec4(deskewed_position, point.position.w);
    }
}
