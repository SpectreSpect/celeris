#include "lidar_message_odometry_recorder.h"

LidarMessageOdometryRecorder::~LidarMessageOdometryRecorder() noexcept {
    stop();
}

void LidarMessageOdometryRecorder::start(std::filesystem::path path) {
    LOG_METHOD();

    logger().check(!m_is_recording, "Recording has already been started");
    logger().check(!path.empty(), "Recording path was empty");
    logger().check(
        std::filesystem::is_directory(path),
        "Recording path does not point to an existing directory: " + path.string()
    );
    logger().check(
        std::filesystem::is_empty(path),
        "Recording directory was not empty: " + path.string()
    );

    const auto index_path = path / "index.bin";

    m_index_file.open(
        index_path,
        std::ios::binary | std::ios::trunc
    );

    logger().check(
        m_index_file.is_open(),
        "Failed to create index file: " + index_path.string()
    );

    m_recording_dir = std::move(path);
    m_is_recording = true;
    m_record_count = 0;

    m_index_file.write(
        reinterpret_cast<const char*>(&m_record_count),
        static_cast<std::streamsize>(sizeof(m_record_count))
    );

    logger().check(
        static_cast<bool>(m_index_file),
        "Failed to write initial record count"
    );

    // update_record_count();
}

void LidarMessageOdometryRecorder::stop() {
    LOG_METHOD();

    logger().check(
        m_is_recording,
        "Cannot stop because recording has not been started"
    );

    m_index_file.flush();

    logger().check(
        static_cast<bool>(m_index_file),
        "Failed to flush index file"
    );

    m_index_file.close();

    m_is_recording = false;
    m_record_count = 0;
    m_recording_dir.clear();
}

void LidarMessageOdometryRecorder::record(const LidarMessage& lidar_msg, const Odometry& odometry) {
    LOG_METHOD();

    logger().check(m_is_recording, "Cannot record before recording has been started");
    logger().check(m_index_file.is_open(), "The index file wasn't open");

    std::string scan_file_name = "scan_" + std::to_string(m_record_count) + ".lmb";
    std::filesystem::path scan_file_path = m_recording_dir / scan_file_name;

    lidar_msg.save(scan_file_path);
    
    m_index_file.write(
        reinterpret_cast<const char*>(&m_record_count), 
        static_cast<std::streamsize>(sizeof(m_record_count))
    );
    logger().check(static_cast<bool>(m_index_file), "Failed to write entry index to file");

    m_index_file.write(
        reinterpret_cast<const char*>(&odometry), 
        static_cast<std::streamsize>(sizeof(odometry))
    );
    logger().check(static_cast<bool>(m_index_file), "Failed to write odometry to file");

    m_index_file.flush();

    logger().check(
        static_cast<bool>(m_index_file),
        "Failed to flush index file"
    );

    m_record_count++;

    update_record_count();
}

bool LidarMessageOdometryRecorder::is_recording() const noexcept {
    return m_is_recording;
}

void LidarMessageOdometryRecorder::update_record_count() {
    m_index_file.flush();

    const auto current_position = m_index_file.tellp();

    m_index_file.seekp(0, std::ios::beg);
    m_index_file.write(
        reinterpret_cast<const char*>(&m_record_count),
        sizeof(m_record_count)
    );

    m_index_file.seekp(current_position);

    logger().check(
        static_cast<bool>(m_index_file),
        "Failed to update record count"
    );
}