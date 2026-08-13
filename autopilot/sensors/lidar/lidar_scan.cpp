#include "lidar_scan.h"

#include "../../../renderer/point_cloud/point_cloud_preprocessor.h"
#include "../../../managers/manager_bundle.h"


LidarScan::LidarScan(
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

    m_timestamp = message.timestamps[0];
    
    add_child(m_point_cloud);
}


LidarScan::LidarScan(
    ManagerBundle& manager_bundle, 
    const std::vector<PointInstance>& points,
    const std::vector<glm::vec4>& normals,
    uint64_t timestamp)   
    :   m_manager_bundle(&manager_bundle),
        m_point_cloud(manager_bundle, points),
        m_normal_buffer(
           VulkanBuffer::create_host_visible_storage_buffer(
               manager_bundle.engine(), 
               m_point_cloud.point_count() * sizeof(glm::vec4)
           )
        ),
        m_timestamp(timestamp)
{
    LOG_METHOD();
    m_normal_buffer.upload(normals.data(), normals.size() * sizeof(glm::vec4), 0);
    add_child(m_point_cloud);
}

LidarScan::LidarScan(ManagerBundle& manager_bundle, std::filesystem::path path) 
    :   LidarScan(load(manager_bundle, path)) {}

PointCloud LidarScan::point_cloud_from_lidar_msg(LidarMessage&& message) {
    LOG_METHOD();

    logger().check(m_manager_bundle, "Manager bundle was null");
    logger().check(!message.points.empty(), "Lidar message had no points");

    return PointCloud(*m_manager_bundle, std::move(message.points));
}

void LidarScan::save(std::filesystem::path path) {
    LOG_METHOD();

    logger().check(!path.empty(), "Output path was empty");
    logger().check(path.has_filename(), "Output path has no filename");

    const auto parent = path.parent_path();

    logger().check(
        parent.empty() || std::filesystem::is_directory(parent),
        "Output folder does not exist: " + parent.string()
    );

    uint64_t point_count = m_point_cloud.point_count();
    uint64_t normal_count = m_normal_buffer.size() / sizeof(glm::vec4);

    logger().check(
        point_count > 0,
        "Lidar scan didn't have any points"
    );

    logger().check(
        point_count == normal_count,
        "Normal count did not match point count"
    );

    std::ofstream out(path, std::ios::binary | std::ios::trunc);

    logger().check(
        out.is_open(),
        "Failed to create file: " + path.string()
    );

    std::vector<PointInstance> points(point_count);
    std::vector<glm::vec4> normals(point_count);

    m_point_cloud.instance_buffer().read(points.data(), sizeof(PointInstance) * point_count, 0);
    m_normal_buffer.read(normals.data(), sizeof(glm::vec4) * normals.size(), 0);

    
    out.write(
        reinterpret_cast<const char*>(&point_count),
        static_cast<std::streamsize>(sizeof(point_count))
    );

    logger().check(
        out.good(),
        "Failed to write point count to file: " + path.string()
    );

    out.write(
        reinterpret_cast<const char*>(&m_timestamp),
        static_cast<std::streamsize>(sizeof(m_timestamp))
    );

    logger().check(
        out.good(),
        "Failed to write timestamp to file: " + path.string()
    );

    out.write(
        reinterpret_cast<const char*>(points.data()),
        static_cast<std::streamsize>(sizeof(PointInstance) * point_count)
    );

    logger().check(
        out.good(),
        "Failed to write points to file: " + path.string()
    );

    out.write(
        reinterpret_cast<const char*>(normals.data()),
        static_cast<std::streamsize>(sizeof(glm::vec4) * point_count)
    );

    logger().check(
        out.good(),
        "Failed to write normals to file: " + path.string()
    );
}

LidarScan LidarScan::load(ManagerBundle& manager_bundle, std::filesystem::path path) {
    LOG_NAMED("LidarScan");
    
    logger().check(!path.empty(), "Output path was empty");
    logger().check(path.has_filename(), "Output path has no filename");

    const auto parent = path.parent_path();

    logger().check(
        parent.empty() || std::filesystem::is_directory(parent),
        "Output folder does not exist: " + parent.string()
    );

    std::ifstream file(path, std::ios::binary);

    logger().check(
        file.is_open(),
        "Failed to open file: " + path.string()
    );

    uint64_t point_count = 0;
    file.read(
        reinterpret_cast<char*>(&point_count),
        static_cast<std::streamsize>(sizeof(uint64_t))
    );

    logger().check(
        static_cast<bool>(file),
        "Failed to read point count from: " + path.string()
    );

    logger().check(
        point_count > 0,
        "Point count was 0"
    );

    logger().check(
        point_count <= std::numeric_limits<std::size_t>::max(),
        "Point count is too large"
    );

    uint64_t timestamp = 0;
    file.read(
        reinterpret_cast<char*>(&timestamp),
        static_cast<std::streamsize>(sizeof(uint64_t))
    );

    logger().check(
        static_cast<bool>(file),
        "Failed to read timestamp from: " + path.string()
    );
    
    std::vector<PointInstance> points(static_cast<std::size_t>(point_count));
    std::vector<glm::vec4> normals(static_cast<std::size_t>(point_count));

    file.read(
        reinterpret_cast<char*>(points.data()),
        static_cast<std::streamsize>(sizeof(PointInstance) * point_count)
    );

    logger().check(
        static_cast<bool>(file),
        "Failed to read point data from: " + path.string()
    );

    file.read(
        reinterpret_cast<char*>(normals.data()),
        static_cast<std::streamsize>(sizeof(glm::vec4) * point_count)
    );

    logger().check(
        static_cast<bool>(file),
        "Failed to read normal data from: " + path.string()
    );
    
    return LidarScan(manager_bundle, points, normals, timestamp);
}

PointCloud& LidarScan::point_cloud() noexcept {
    return m_point_cloud;
}

VulkanBuffer& LidarScan::normal_buffer() noexcept {
    return m_normal_buffer;
}

uint64_t LidarScan::timestamp() const noexcept {
    return m_timestamp;
}