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
constexpr size_t imu_floats_per_scan_point = 14;
constexpr uint32_t max_points_per_frame = 2'000'000;

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

glm::mat3 sample_orientation_matrix(const LidarScan::FrameData& frame, size_t sample_id) {
    if (frame.sample_orientations.size() == frame.samples.size()) {
        glm::quat q = frame.sample_orientations[sample_id];
        const float len = glm::length(q);
        if (std::isfinite(len) && len > 1e-6f)
            return glm::mat3_cast(glm::normalize(q));
    }

    return glm::mat3(1.0f);
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
    const glm::mat3 rotation = sample_orientation_matrix(frame, ref_idx);

    std::unique_ptr<LidarScan> scan = std::make_unique<LidarScan>(
        manager_bundle,
        *m_point_cloud_preprocessor,
        std::move(frame)
    );

    scan->point_cloud().transform.position = glm::vec3(0.0f);
    scan->point_cloud().transform.rotation = glm::quat_cast(rotation);
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
    LOG_METHOD();

    while (m_running.load()) {
        LidarScan::FrameData frame;

        LidarScanMessageHeader header;

        if (!read_exact(client_socket, &header, sizeof(LidarScanMessageHeader)))
            return false;
        
        frame.timestamp_ns = header.timestamp_ns;

        const uint32_t point_count = header.point_count;

        if (point_count == 0 || point_count > max_points_per_frame) {
            logger().log_error("invalid point count");
            return false;
        }

        std::vector<PointData> message_points(point_count);
        if (!read_exact(
            client_socket, 
            message_points.data(), 
            message_points.size() * sizeof(PointData))) {
            return false;
        }

        frame.ring_count = m_ring_count;

        frame.samples.resize(point_count / m_points_freq);
        frame.sample_orientations.resize(frame.samples.size());
        frame.sample_linear_accelerations.resize(frame.samples.size());
        frame.sample_angular_velocities.resize(frame.samples.size());
        
        uint32_t valid_count = 0;

        for (uint32_t i = 0; i < point_count / m_points_freq; ++i) {
            uint32_t point_id = i * m_points_freq;
            PointData& point_data = message_points[point_id];

            frame.sample_linear_accelerations[i] = point_data.linear_acceleration;
            frame.sample_angular_velocities[i] = point_data.angular_velocity;
            frame.sample_orientations[i] = normalized_quat_or_identity(
                point_data.orientation.x,
                point_data.orientation.y,
                point_data.orientation.z,
                point_data.orientation.w
            );
            frame.samples[i].p_local = point_data.position;
            frame.samples[i].time = point_data.point_time;
            frame.samples[i].valid = 
                std::isfinite(point_data.position.x) && 
                std::isfinite(point_data.position.y) && 
                std::isfinite(point_data.position.z);

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
