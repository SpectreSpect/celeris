#include "lidar_scan_receiver.h"

#include "lidar_message_point_data.h"
#include "lidar_message_header.h"
#include "lidar_scan.h"

LidarScanReceiver::LidarScanReceiver(
    ManagerBundle& manager_bundle,
    PointCloudPreprocessor& point_cloud_preprocessor,
    uint16_t port,
    size_t max_queued_message) 
    :   ReceiverServer<LidarMessage>(port, max_queued_message),
        m_manager_bundle(&manager_bundle),
        m_point_cloud_preprocessor(&point_cloud_preprocessor) {
}

LidarScanReceiver::~LidarScanReceiver() {
    stop();
}

std::unique_ptr<LidarScan> LidarScanReceiver::try_get_lidar_scan_from_lidar_msg(
    LidarMessage& message) 
{
    LOG_METHOD();

    logger().check(m_manager_bundle, "Manager bundle was null");
    logger().check(m_point_cloud_preprocessor, "Point cloud preprocessor was null");

    // if (!try_pop_front_lidar_msg(message))
    //     return nullptr;
    
    if (message.points.empty() || message.timestamps.empty())
        return nullptr;
    
    std::unique_ptr<LidarScan> scan = std::make_unique<LidarScan>(
        *m_manager_bundle,
        *m_point_cloud_preprocessor,
        message
    );

    return scan;
}

std::unique_ptr<LidarScan> LidarScanReceiver::try_pop_front_lidar_scan() {
    LOG_METHOD();

    logger().check(m_manager_bundle, "Manager bundle was null");
    logger().check(m_point_cloud_preprocessor, "Point cloud preprocessor was null");
    
    LidarMessage message;

    if (!try_pop_front(message))
        return nullptr;
    
    // if (message.points.empty() || message.timestamps.empty())
    //     return nullptr;
    
    
    // std::unique_ptr<LidarScan> scan = std::make_unique<LidarScan>(
    //     *m_manager_bundle,
    //     *m_point_cloud_preprocessor,
    //     std::move(message)
    // );

    std::unique_ptr<LidarScan> scan = try_get_lidar_scan_from_lidar_msg(message);

    return scan;
}

bool LidarScanReceiver::read_exact_message(int client_socket, LidarMessage& message) {
    LidarMessageHeader header{};

    if (!read_exact(client_socket, &header, sizeof(LidarMessageHeader)))
        return false;
    
    if (header.point_count == 0 || header.point_count > m_max_points_per_message) {
        logger().log_error("invalid point count");
        return false;
    }

    std::vector<LidarMessagePointData> points(header.point_count);
    if (!read_exact(client_socket, points.data(), sizeof(LidarMessagePointData) * header.point_count))
        return false;

    // LidarMessage lidar_message;
    message.scan_timestamp = header.timestamp_ns;
    message.points.reserve(header.point_count);
    message.timestamps.reserve(header.point_count);

    uint64_t latest_time_offset_ns = points[0].time_offset_ns;
    // for (LidarMessagePointData& point_data : points) {
    for (LidarMessagePointData& point_data : points) {
        message.points.push_back(PointInstance{
            .position = glm::vec4(point_data.position, 1.0f),
            .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
        });
        message.timestamps.push_back(header.timestamp_ns + point_data.time_offset_ns);

        if (latest_time_offset_ns <= point_data.time_offset_ns) {
            latest_time_offset_ns = point_data.time_offset_ns;
            message.latest_point_id = message.points.size() - 1;
        }
    }

    return true;
}
