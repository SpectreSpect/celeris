#include "lidar_message.h"

LidarMessage::LidarMessage(std::filesystem::path path) 
    :   LidarMessage(load(path)) {}

void LidarMessage::save(std::filesystem::path path) {
    LOG_METHOD();

    logger().check(!path.empty(), "Output path was empty");
    logger().check(path.has_filename(), "Output path has no filename");

    const auto parent = path.parent_path();

    logger().check(
        parent.empty() || std::filesystem::is_directory(parent),
        "Output folder does not exist: " + parent.string()
    );

    uint64_t point_count = points.size();
    uint64_t timestamp_count = timestamps.size();

    logger().check(
        point_count > 0,
        "Lidar message didn't have any points"
    );

    logger().check(
        point_count == timestamp_count,
        "Timestamp count did not match point count"
    );

    std::ofstream out(path, std::ios::binary | std::ios::trunc);

    logger().check(
        out.is_open(),
        "Failed to create file: " + path.string()
    );

    // std::vector<PointInstance> points(point_count);
    // std::vector<glm::vec4> normals(point_count);

    // m_point_cloud.instance_buffer().read(points.data(), sizeof(PointInstance) * point_count, 0);
    // m_normal_buffer.read(normals.data(), sizeof(glm::vec4) * normals.size(), 0);

    
    out.write(
        reinterpret_cast<const char*>(&point_count),
        static_cast<std::streamsize>(sizeof(point_count))
    );

    logger().check(
        out.good(),
        "Failed to write point count to file: " + path.string()
    );

    // out.write(
    //     reinterpret_cast<const char*>(&m_timestamp),
    //     static_cast<std::streamsize>(sizeof(m_timestamp))
    // );

    // logger().check(
    //     out.good(),
    //     "Failed to write timestamp to file: " + path.string()
    // );

    out.write(
        reinterpret_cast<const char*>(points.data()),
        static_cast<std::streamsize>(sizeof(PointInstance) * point_count)
    );

    logger().check(
        out.good(),
        "Failed to write points to file: " + path.string()
    );

    out.write(
        reinterpret_cast<const char*>(timestamps.data()),
        static_cast<std::streamsize>(sizeof(uint64_t) * point_count)
    );

    logger().check(
        out.good(),
        "Failed to write timestamps to file: " + path.string()
    );
}

LidarMessage LidarMessage::load(std::filesystem::path path) {
    LOG_NAMED("LidarMessage");
    
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

    // uint64_t timestamp = 0;
    // file.read(
    //     reinterpret_cast<char*>(&timestamp),
    //     static_cast<std::streamsize>(sizeof(uint64_t))
    // );

    // logger().check(
    //     static_cast<bool>(file),
    //     "Failed to read timestamp from: " + path.string()
    // );
    
    std::vector<PointInstance> points(static_cast<std::size_t>(point_count));
    std::vector<uint64_t> timestamps(static_cast<std::size_t>(point_count));

    file.read(
        reinterpret_cast<char*>(points.data()),
        static_cast<std::streamsize>(sizeof(PointInstance) * point_count)
    );

    logger().check(
        static_cast<bool>(file),
        "Failed to read point data from: " + path.string()
    );

    file.read(
        reinterpret_cast<char*>(timestamps.data()),
        static_cast<std::streamsize>(sizeof(uint64_t) * point_count)
    );

    logger().check(
        static_cast<bool>(file),
        "Failed to read timestamp data from: " + path.string()
    );

    LidarMessage lidar_msg;
    lidar_msg.points = std::move(points);
    lidar_msg.timestamps = std::move(timestamps);
    
    return lidar_msg;
}