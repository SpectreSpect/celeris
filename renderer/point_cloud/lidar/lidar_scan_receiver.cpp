#include "lidar_scan_receiver.h"

#include "../../../managers/manager_bundle.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#include <glm/gtc/quaternion.hpp>

namespace {
constexpr uint32_t imu_rpy_frame_flag = 1u << 31;
constexpr uint32_t imu_pose_quat_frame_flag = 1u << 30;
constexpr uint32_t imu_accel_quat_frame_flag = 1u << 29;
constexpr uint32_t point_count_mask = ~(imu_rpy_frame_flag | imu_pose_quat_frame_flag | imu_accel_quat_frame_flag);
constexpr size_t gps_imu_floats_per_scan_point = 10;
constexpr size_t imu_rpy_floats_per_scan_point = 7;
constexpr size_t imu_pose_quat_floats_per_scan_point = 11;
constexpr size_t imu_accel_quat_floats_per_scan_point = 11;
constexpr uint32_t max_points_per_frame = 2'000'000;

enum class ScanPacketFormat {
    LegacyGpsRpy,
    ImuRpy,
    ImuPoseQuat,
    ImuAccelQuat
};

ScanPacketFormat packet_format(uint32_t count_header) {
    if ((count_header & imu_accel_quat_frame_flag) != 0u)
        return ScanPacketFormat::ImuAccelQuat;
    if ((count_header & imu_pose_quat_frame_flag) != 0u)
        return ScanPacketFormat::ImuPoseQuat;
    if ((count_header & imu_rpy_frame_flag) != 0u)
        return ScanPacketFormat::ImuRpy;
    return ScanPacketFormat::LegacyGpsRpy;
}

size_t bytes_per_scan_point(ScanPacketFormat format) {
    switch (format) {
    case ScanPacketFormat::ImuAccelQuat:
        return imu_accel_quat_floats_per_scan_point * sizeof(float);
    case ScanPacketFormat::ImuPoseQuat:
        return imu_pose_quat_floats_per_scan_point * sizeof(float);
    case ScanPacketFormat::ImuRpy:
        return imu_rpy_floats_per_scan_point * sizeof(float);
    case ScanPacketFormat::LegacyGpsRpy:
    default:
        return gps_imu_floats_per_scan_point * sizeof(float);
    }
}

glm::mat3 ros_basis_to_engine_basis() {
    glm::mat3 m(1.0f);
    m[0] = glm::vec3(-1.0f, 0.0f, 0.0f);
    m[1] = glm::vec3( 0.0f, 0.0f, 1.0f);
    m[2] = glm::vec3( 0.0f, 1.0f, 0.0f);
    return m;
}

size_t reference_sample_id(const LidarScan::FrameData& frame) {
    size_t ref_idx = 0;
    float min_time = std::numeric_limits<float>::infinity();

    for (size_t i = 0; i < frame.samples.size(); ++i) {
        if (frame.samples[i].time < min_time) {
            min_time = frame.samples[i].time;
            ref_idx = i;
        }
    }

    return ref_idx;
}

glm::quat normalized_quat_or_identity(float qx, float qy, float qz, float qw) {
    glm::quat q(qw, qx, qy, qz);
    const float len = glm::length(q);
    if (!std::isfinite(len) || len <= 1e-6f)
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    return glm::normalize(q);
}

glm::mat3 sample_orientation_matrix_ros(const LidarScan::FrameData& frame, size_t sample_id) {
    if (frame.sample_orientations_ros.size() == frame.samples.size()) {
        glm::quat q = frame.sample_orientations_ros[sample_id];
        const float len = glm::length(q);
        if (std::isfinite(len) && len > 1e-6f)
            return glm::mat3_cast(glm::normalize(q));
    }

    const auto& sample = frame.samples[sample_id];
    return LidarScan::rpy_to_mat3_zyx(
        sample.base_rpy_ros.x,
        sample.base_rpy_ros.y,
        sample.base_rpy_ros.z
    );
}
}

LidarScanReceiver::LidarScanReceiver(
    PointCloudPreprocessor& point_cloud_preprocessor, 
    uint16_t port,
    size_t max_queued_frames,
    uint32_t points_freq)
    :   m_point_cloud_preprocessor(&point_cloud_preprocessor), 
        m_port(port), 
        m_max_queued_frames(max_queued_frames),
        m_points_freq(points_freq) {}

LidarScanReceiver::~LidarScanReceiver() {
    stop();
}

void LidarScanReceiver::start() {
    if (m_running.exchange(true)) {
        return;
    }

    m_thread = std::thread(&LidarScanReceiver::receive_loop, this);
}

void LidarScanReceiver::stop() {
    if (!m_running.exchange(false)) {
        return;
    }

    close_listen_socket();

    int client_socket = m_client_socket.exchange(-1);
    if (client_socket >= 0) {
        close(client_socket);
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void LidarScanReceiver::save_retrieved_scan(
        const void* data, 
        size_t size_bytes, 
        const std::filesystem::path& path) {
    std::ofstream out(path.string());
    out.write((const char*)data, size_bytes);
    out.close();
}

// void LidarScanReceiver::save_frame_data(
//         const LidarScan::FrameData& frame_data, 
//         const std::filesystem::path& path) {
//     std::ofstream out(path.string());

//     out.write((const char*)&frame_data.timestamp_ns, sizeof(frame_data.timestamp_ns));
//     out.write((const char*)&frame_data.ring_count, sizeof(frame_data.ring_count));
//     out.write((const char*)frame_data.samples.data(), frame_data.samples.size() * sizeof(LidarScan::TimedPointSample));
//     out.write((const char*)frame_data.points.data(), frame_data.points.size() * sizeof(PointInstance));

//     out.close();
// }

bool LidarScanReceiver::try_pop_frame(LidarScan::FrameData& frame) {
    std::lock_guard<std::mutex> lock(m_queue_mutex);

    if (m_frames.empty()) {
        return false;
    }

    frame = std::move(m_frames.front());
    m_frames.pop_front();
    return true;
}

std::unique_ptr<LidarScan> LidarScanReceiver::try_pop_scan(ManagerBundle& manager_bundle) {
    LidarScan::FrameData frame;

    if (!try_pop_frame(frame))
        return nullptr;

    if (!m_point_cloud_preprocessor)
        return nullptr;

    if (frame.samples.empty())
        return nullptr;

    const size_t ref_idx = reference_sample_id(frame);
    const auto& sample = frame.samples[ref_idx];
    const glm::vec3 base_pos_ros = sample.base_pos_ros;
    const glm::mat3 rotation_ros = sample_orientation_matrix_ros(frame, ref_idx);
    const glm::mat3 basis = ros_basis_to_engine_basis();
    const glm::mat3 rotation_engine = basis * rotation_ros * glm::transpose(basis);

    std::unique_ptr<LidarScan> scan = std::make_unique<LidarScan>(
        manager_bundle,
        *m_point_cloud_preprocessor,
        std::move(frame)
    );

    scan->point_cloud().transform.position = LidarScan::ros_pos_to_engine(base_pos_ros);
    scan->point_cloud().transform.rotation = glm::quat_cast(rotation_engine);
    // scan->point_cloud().transform.scale = glm::vec3(2);

    return scan;
}

bool LidarScanReceiver::is_running() const noexcept {
    return m_running.load();
}

void LidarScanReceiver::receive_loop() {
    m_listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_listen_socket < 0) {
        std::cerr << "LidarScanReceiver: socket failed\n";
        m_running = false;
        return;
    }

    int yes = 1;
    setsockopt(m_listen_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(m_port);

    if (bind(m_listen_socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "LidarScanReceiver: bind failed on port " << m_port << "\n";
        close_listen_socket();
        m_running = false;
        return;
    }

    if (listen(m_listen_socket, 1) < 0) {
        std::cerr << "LidarScanReceiver: listen failed\n";
        close_listen_socket();
        m_running = false;
        return;
    }

    std::cout << "LidarScanReceiver: listening on port " << m_port << "\n";

    while (m_running.load()) {
        sockaddr_in client_address{};
        socklen_t client_len = sizeof(client_address);

        int client_socket = accept(
            m_listen_socket,
            reinterpret_cast<sockaddr*>(&client_address),
            &client_len
        );

        if (client_socket < 0) {
            if (m_running.load()) {
                std::cerr << "LidarScanReceiver: accept failed\n";
            }
            continue;
        }

        std::cout << "LidarScanReceiver: client connected\n";
        m_client_socket = client_socket;
        receive_frames_from_client(client_socket);
        if (m_client_socket.exchange(-1) == client_socket) {
            close(client_socket);
        }
        std::cout << "LidarScanReceiver: client disconnected\n";
    }

    close_listen_socket();
}

bool LidarScanReceiver::receive_frames_from_client(int client_socket) {
    while (m_running.load()) {
        LidarScan::FrameData frame;
        uint32_t point_count_header = 0;

        if (!read_exact(client_socket, &frame.timestamp_ns, sizeof(frame.timestamp_ns))) {
            return false;
        }

        if (!read_exact(client_socket, &point_count_header, sizeof(point_count_header))) {
            return false;
        }

        const ScanPacketFormat format = packet_format(point_count_header);
        const uint32_t point_count = point_count_header & point_count_mask;

        if (point_count == 0 || point_count > max_points_per_frame) {
            std::cerr << "LidarScanReceiver: invalid point count " << point_count << "\n";
            return false;
        }

        const size_t point_stride_bytes = bytes_per_scan_point(format);
        std::vector<uint8_t> payload(static_cast<size_t>(point_count) * point_stride_bytes);
        if (!read_exact(client_socket, payload.data(), payload.size())) {
            return false;
        }

        frame.ring_count = 16;

        frame.samples.resize(point_count / m_points_freq);
        if (format == ScanPacketFormat::ImuPoseQuat || format == ScanPacketFormat::ImuAccelQuat) {
            frame.sample_orientations_ros.resize(frame.samples.size());
        }
        if (format == ScanPacketFormat::ImuAccelQuat) {
            frame.sample_linear_accelerations_ros.resize(frame.samples.size());
        }

        
        // save_retrieved_scan(payload.data(), payload.size(), "/home/hiber/repositories/celeris/assets/lidar_scans/lslidar_scan.bin");

        const uint8_t* p = payload.data();

        uint32_t valid_count = 0;

        for (uint32_t i = 0; i < point_count / m_points_freq; ++i) {
            float x, y, z;
            float time;
            float px, py, pz;
            float roll, pitch, yaw;
            float qx, qy, qz, qw;
            float ax, ay, az;

            const uint8_t* local_p = p;
            std::memcpy(&x,     local_p, 4); local_p += 4;
            std::memcpy(&y,     local_p, 4); local_p += 4;
            std::memcpy(&z,     local_p, 4); local_p += 4;
            std::memcpy(&time,  local_p, 4); local_p += 4;
            if (format == ScanPacketFormat::ImuRpy) {
                px = 0.0f;
                py = 0.0f;
                pz = 0.0f;
                std::memcpy(&roll,  local_p, 4); local_p += 4;
                std::memcpy(&pitch, local_p, 4); local_p += 4;
                std::memcpy(&yaw,   local_p, 4); local_p += 4;
            } else if (format == ScanPacketFormat::ImuPoseQuat) {
                std::memcpy(&px, local_p, 4); local_p += 4;
                std::memcpy(&py, local_p, 4); local_p += 4;
                std::memcpy(&pz, local_p, 4); local_p += 4;
                std::memcpy(&qx, local_p, 4); local_p += 4;
                std::memcpy(&qy, local_p, 4); local_p += 4;
                std::memcpy(&qz, local_p, 4); local_p += 4;
                std::memcpy(&qw, local_p, 4); local_p += 4;
                roll = 0.0f;
                pitch = 0.0f;
                yaw = 0.0f;
                frame.sample_orientations_ros[i] = normalized_quat_or_identity(qx, qy, qz, qw);
            } else if (format == ScanPacketFormat::ImuAccelQuat) {
                std::memcpy(&ax, local_p, 4); local_p += 4;
                std::memcpy(&ay, local_p, 4); local_p += 4;
                std::memcpy(&az, local_p, 4); local_p += 4;
                std::memcpy(&qx, local_p, 4); local_p += 4;
                std::memcpy(&qy, local_p, 4); local_p += 4;
                std::memcpy(&qz, local_p, 4); local_p += 4;
                std::memcpy(&qw, local_p, 4); local_p += 4;
                px = 0.0f;
                py = 0.0f;
                pz = 0.0f;
                roll = 0.0f;
                pitch = 0.0f;
                yaw = 0.0f;
                frame.sample_linear_accelerations_ros[i] = glm::vec3(ax, ay, az);
                frame.sample_orientations_ros[i] = normalized_quat_or_identity(qx, qy, qz, qw);
            } else {
                std::memcpy(&px, local_p, 4); local_p += 4;
                std::memcpy(&py, local_p, 4); local_p += 4;
                std::memcpy(&pz, local_p, 4); local_p += 4;
                std::memcpy(&roll,  local_p, 4); local_p += 4;
                std::memcpy(&pitch, local_p, 4); local_p += 4;
                std::memcpy(&yaw,   local_p, 4); local_p += 4;
            }
            p += point_stride_bytes * m_points_freq;

            // std::cout << "Point: " << time << std::endl;

            frame.samples[i].p_local_ros = glm::vec3(x, y, z);
            frame.samples[i].time = time;
            frame.samples[i].base_pos_ros = glm::vec3(px, py, pz);
            frame.samples[i].base_rpy_ros = glm::vec3(roll, pitch, yaw);
            frame.samples[i].valid = std::isfinite(x) && std::isfinite(y) && std::isfinite(z);

            if (frame.samples[i].valid)
                valid_count++;
        }

        LidarScan::build_points_for_frame(frame);

        if (valid_count > 0 && !frame.points.empty()) {
            // frame.save("/home/hiber/repositories/celeris/assets/lidar_scans/ouster_scan.bin");
            push_frame(std::move(frame));
        }
    }

    return true;
}

bool LidarScanReceiver::read_exact(int socket, void* data, size_t byte_count) {
    auto* bytes = static_cast<uint8_t*>(data);
    size_t bytes_read = 0;

    while (bytes_read < byte_count && m_running.load()) {
        ssize_t n = recv(socket, bytes + bytes_read, byte_count - bytes_read, 0);

        if (n <= 0) {
            return false;
        }

        bytes_read += static_cast<size_t>(n);
    }

    return bytes_read == byte_count;
}

void LidarScanReceiver::push_frame(LidarScan::FrameData frame) {
    std::lock_guard<std::mutex> lock(m_queue_mutex);

    while (m_frames.size() >= m_max_queued_frames) {
        m_frames.pop_front();
    }

    m_frames.push_back(std::move(frame));
}

void LidarScanReceiver::close_listen_socket() {
    if (m_listen_socket >= 0) {
        close(m_listen_socket);
        m_listen_socket = -1;
    }
}
