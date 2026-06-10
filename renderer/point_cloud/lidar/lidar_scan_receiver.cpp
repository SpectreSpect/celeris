#include "lidar_scan_receiver.h"

#include "../../../managers/manager_bundle.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
constexpr size_t bytes_per_scan_point = 10 * sizeof(float);
constexpr uint32_t max_points_per_frame = 2'000'000;

glm::mat3 ros_basis_to_engine_basis() {
    glm::mat3 m(1.0f);
    m[0] = glm::vec3(-1.0f, 0.0f, 0.0f);
    m[1] = glm::vec3( 0.0f, 0.0f, 1.0f);
    m[2] = glm::vec3( 0.0f, 1.0f, 0.0f);
    return m;
}
}

LidarScanReceiver::LidarScanReceiver(PointCloudPreprocessor& point_cloud_preprocessor, 
                                     uint16_t port, size_t max_queued_frames)
    :   m_point_cloud_preprocessor(&point_cloud_preprocessor), 
        m_port(port), 
        m_max_queued_frames(max_queued_frames) {}

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

    const auto& sample = frame.samples.front();
    const glm::vec3 base_pos_ros = sample.base_pos_ros;
    const glm::vec3 base_rpy_ros = sample.base_rpy_ros;

    std::unique_ptr<LidarScan> scan = std::make_unique<LidarScan>(
        manager_bundle,
        *m_point_cloud_preprocessor,
        std::move(frame)
    );

    scan->point_cloud().transform.position = LidarScan::ros_pos_to_engine(base_pos_ros);

    glm::mat3 rotation_ros = LidarScan::rpy_to_mat3_zyx(
        base_rpy_ros.x,
        base_rpy_ros.y,
        base_rpy_ros.z
    );
    glm::mat3 basis = ros_basis_to_engine_basis();
    glm::mat3 rotation_engine = basis * rotation_ros * glm::transpose(basis);

    scan->point_cloud().transform.rotation = glm::quat_cast(rotation_engine);

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
    address.sin_addr.s_addr = INADDR_ANY;
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
        uint32_t point_count = 0;

        if (!read_exact(client_socket, &frame.timestamp_ns, sizeof(frame.timestamp_ns))) {
            return false;
        }

        if (!read_exact(client_socket, &point_count, sizeof(point_count))) {
            return false;
        }

        if (point_count == 0 || point_count > max_points_per_frame) {
            std::cerr << "LidarScanReceiver: invalid point count " << point_count << "\n";
            return false;
        }

        std::vector<uint8_t> payload(static_cast<size_t>(point_count) * bytes_per_scan_point);
        if (!read_exact(client_socket, payload.data(), payload.size())) {
            return false;
        }

        frame.samples.resize(point_count);
        const uint8_t* p = payload.data();

        uint32_t valid_count = 0;
        for (uint32_t i = 0; i < point_count; ++i) {
            float x, y, z;
            float time;
            float px, py, pz;
            float roll, pitch, yaw;

            std::memcpy(&x,     p, 4); p += 4;
            std::memcpy(&y,     p, 4); p += 4;
            std::memcpy(&z,     p, 4); p += 4;
            std::memcpy(&time,  p, 4); p += 4;
            std::memcpy(&px,    p, 4); p += 4;
            std::memcpy(&py,    p, 4); p += 4;
            std::memcpy(&pz,    p, 4); p += 4;
            std::memcpy(&roll,  p, 4); p += 4;
            std::memcpy(&pitch, p, 4); p += 4;
            std::memcpy(&yaw,   p, 4); p += 4;

            frame.samples[i].p_local_ros = glm::vec3(x, y, z);
            frame.samples[i].time = time;
            frame.samples[i].base_pos_ros = glm::vec3(px, py, pz);
            frame.samples[i].base_rpy_ros = glm::vec3(roll, pitch, yaw);
            frame.samples[i].valid = std::isfinite(x) && std::isfinite(y) && std::isfinite(z);

            if (frame.samples[i].valid)
                valid_count++;
        }

        LidarScan::build_points_for_frame(frame);

        if (valid_count > 0 && !frame.points.empty())
            push_frame(std::move(frame));
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
