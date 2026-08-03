#include "new_lidar_scan.h"

#include "../../../renderer/point_cloud/point_cloud_preprocessor.h"
#include "../../../managers/manager_bundle.h"


NewLidarScan::NewLidarScan(
    ManagerBundle& manager_bundle, 
    PointCloudPreprocessor& point_cloud_preprocessor, 
    LidarMessage&& message)   
    :   m_manager_bundle(&manager_bundle),
        m_point_cloud(point_cloud_from_lidar_msg(std::move(message))),
        m_normal_buffer(
           VulkanBuffer::create_host_visible_storage_buffer(
               manager_bundle.engine(), 
               m_point_cloud.point_count() * sizeof(glm::vec4)
           )
        ) 
{
    LOG_METHOD();
    point_cloud_preprocessor.remove_points_near_origin(
        m_point_cloud.instance_buffer(),
        m_point_cloud.point_count()
    );
    point_cloud_preprocessor.get_normals_from_webots_lidar_point_cloud(
        m_point_cloud.instance_buffer(), 
        m_normal_buffer, 
        m_point_cloud.point_count(),
        16
    );

    logger().check(
        message.latest_point_id >= 0 &&
        message.latest_point_id < message.timestamps.size(),
        "Latest point index was out of bounds"
    );

    m_timestamp = message.timestamps[message.latest_point_id];
    
    add_child(m_point_cloud);
}

PointCloud NewLidarScan::point_cloud_from_lidar_msg(LidarMessage&& message) {
    LOG_METHOD();

    logger().check(m_manager_bundle, "Manager bundle was null");
    logger().check(!message.points.empty(), "Lidar message had no points");

    return PointCloud(*m_manager_bundle, std::move(message.points));
}

PointCloud& NewLidarScan::point_cloud() noexcept {
    return m_point_cloud;
}

VulkanBuffer& NewLidarScan::normal_buffer() noexcept {
    return m_normal_buffer;
}

uint64_t NewLidarScan::timestamp() const noexcept {
    return m_timestamp;
}