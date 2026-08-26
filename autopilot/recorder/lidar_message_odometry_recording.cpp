#include "lidar_message_odometry_recording.h"

void LidarMessageOdometryRecordering::load(
    std::filesystem::path path,
    int first_entry_id,
    int last_entry_id) {
    LOG_METHOD();
     
    logger().check(!path.empty(), "Recording path was empty");
    logger().check(!path.has_filename(), "Recording path must be a directory");

    std::filesystem::path index_path = path / "index.bin";

    logger().check(
        std::filesystem::exists(index_path),
        "Recording directory does not contain index.bin"
    );

    std::ifstream index(index_path, std::ios::binary);

    logger().check(
        index.is_open(),
        "Failed to open file: " + index_path.string()
    );

    uint32_t entry_count = 0;
    index.read(
        reinterpret_cast<char*>(&entry_count),
        static_cast<std::streamsize>(sizeof(uint32_t))
    );

    logger().check(
        entry_count != 0,
        "Entry count was 0"
    );

    logger().check(
        entry_count <= std::numeric_limits<std::uint32_t>::max(),
        "Entry count is too large"
    );

    logger().check(first_entry_id < static_cast<int>(entry_count), "First entry index was out of bounds");
    logger().check(last_entry_id >= first_entry_id, "Last entry index must be greater than first entry index");

    if (first_entry_id < 0) first_entry_id = 0;
    if (last_entry_id < 0) last_entry_id = static_cast<int>(entry_count) - 1;

    const auto entry_size = static_cast<std::streamoff>(sizeof(std::uint32_t) + sizeof(Odometry));
    const auto first_entry_position = 
        static_cast<std::streamoff>(sizeof(std::uint32_t)) + 
        static_cast<std::streamoff>(first_entry_id) * entry_size;
    
    int current_entry_id = first_entry_id;
    while (current_entry_id <= last_entry_id) {

        const auto current_entry_position = static_cast<std::streamoff>(sizeof(std::uint32_t)) + 
            static_cast<std::streamoff>(current_entry_id) * entry_size;

        index.seekg(current_entry_position, std::ios::beg);

        uint32_t entry_id = 0;
        index.read(
            reinterpret_cast<char*>(&entry_id),
            static_cast<std::streamsize>(sizeof(entry_id))
        );

        logger().check(
            static_cast<bool>(index),
            "Failed to read entry index data from: " + path.string()
        );

        LidarMessageOdometryEntry lidar_msg_odom_entry;

        index.read(
            reinterpret_cast<char*>(&lidar_msg_odom_entry.odometry),
            static_cast<std::streamsize>(sizeof(lidar_msg_odom_entry.odometry))
        );

        logger().check(
            static_cast<bool>(index),
            "Failed to read odometry data from: " + path.string()
        );

        std::string scan_filename = "scan_" + std::to_string(entry_id) + ".lmb";
        std::filesystem::path scan_path = path / scan_filename;

        logger().check(
            std::filesystem::exists(scan_path),
            "Recording directory does not contain " + scan_path.string()
        );

        lidar_msg_odom_entry.lidar_message = LidarMessage(scan_path);

        m_entries.push_back(lidar_msg_odom_entry);

        current_entry_id++;
    }
}

LidarMessageOdometryEntry& LidarMessageOdometryRecordering::get_entry(size_t index) {
    LOG_METHOD();

    logger().check(index >= 0 && index < m_entries.size(), "Index was out of bounds");

    return m_entries[index];
}