#include "new_lidar_scan_receiver.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>


#include "../../../renderer/point_cloud/point_cloud_preprocessor.h"
#include "../../../managers/manager_bundle.h"
#include "lidar_message_point_data.h"
#include "lidar_message_header.h"
#include "new_lidar_scan.h"


NewLidarScanReceiver::NewLidarScanReceiver(
    ManagerBundle& manager_bundle,
    PointCloudPreprocessor& point_cloud_preprocessor,
    uint16_t port,
    size_t max_queued_message)
    :   m_manager_bundle(&manager_bundle),
        m_point_cloud_preprocessor(&point_cloud_preprocessor),
        m_port(port),
        m_max_queued_messages(max_queued_message) {}

void NewLidarScanReceiver::start() {
    if (m_running.exchange(true))
        return;

    m_receiver_thread = std::thread(&NewLidarScanReceiver::receiver_loop, this);
}

bool NewLidarScanReceiver::try_pop_front_lidar_msg(LidarMessage& message) {
    {
        std::unique_lock<std::mutex> lock(m_pending_lidar_msg_mtx);
        
        if (m_lidar_msg_queue.empty())
            return false;

        message = std::move(m_lidar_msg_queue.front());
        m_lidar_msg_queue.pop_front();
    }
    
    return true;
}

std::unique_ptr<NewLidarScan> NewLidarScanReceiver::try_pop_front_lidar_scan() {
    LOG_METHOD();

    logger().check(m_manager_bundle, "Manager bundle was null");
    logger().check(m_point_cloud_preprocessor, "Point cloud preprocessor was null");
    
    LidarMessage message;

    if (!try_pop_front_lidar_msg(message))
        return nullptr;
    
    if (message.points.empty() || message.timestamps.empty())
        return nullptr;
    
    
    
    std::unique_ptr<NewLidarScan> scan = std::make_unique<NewLidarScan>(
        *m_manager_bundle,
        *m_point_cloud_preprocessor,
        std::move(message)
    );

    return scan;
}

void NewLidarScanReceiver::close_listen_socket() {
    if (m_listen_socket > 0) {
        close(m_listen_socket);
        m_listen_socket = -1;
    }
}

bool NewLidarScanReceiver::read_exact(int socket, void* data, size_t byte_count) {
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

void NewLidarScanReceiver::push_back_lidar_msg(LidarMessage& message) {
    LOG_METHOD();
    {
        std::unique_lock<std::mutex> lock(m_pending_lidar_msg_mtx);

        if (m_lidar_msg_queue.size() == m_max_queued_messages)
            m_lidar_msg_queue.pop_front();
        m_lidar_msg_queue.push_back(message);
    }
}

bool NewLidarScanReceiver::receive_lidar_msg_from_client(int client_socket) {
    LOG_METHOD();

    while (m_running.load()) {

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

        LidarMessage lidar_message;
        lidar_message.points.reserve(header.point_count);
        lidar_message.timestamps.reserve(header.point_count);

        uint64_t latest_timestamp = points[0].timestamp;
        // for (LidarMessagePointData& point_data : points) {
        for (LidarMessagePointData& point_data : points) {
            lidar_message.points.push_back(PointInstance{
                .position = glm::vec4(point_data.position, 1.0f),
                .color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
            });
            lidar_message.timestamps.push_back(point_data.timestamp);

            if (latest_timestamp <= point_data.timestamp) {
                latest_timestamp = point_data.timestamp;
                lidar_message.latest_point_id = lidar_message.points.size() - 1;
            }
        }


        
        // LidarMessage lidar_message{};

        // if (!read_exact(client_socket, &lidar_message, sizeof(LidarMessage)))
        //     return false;

        push_back_lidar_msg(lidar_message);
    }

    return true;
}

void NewLidarScanReceiver::receiver_loop() {
    LOG_METHOD();

    m_listen_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (m_listen_socket < 0) {
        logger().log_error("Socket failed");
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
        logger().log_error() << "Bind failed on port " << std::to_string(m_port) << "\n";
        close_listen_socket();
        m_running = false;
        return;
    }

    if (listen(m_listen_socket, 1) < 0) {
        logger().log_error("Listen failed");
        close_listen_socket();
        m_running = false;
        return;
    }

    logger().log() << "NewLidarScanReceiver: Listening on port " << std::to_string(m_port) << "\n";

    while (m_running.load()) {
        sockaddr_in client_address{};
        socklen_t client_len = sizeof(client_address);

        int client_socket = accept(
            m_listen_socket,
            reinterpret_cast<sockaddr*>(&client_address),
            &client_len
        );

        if (client_socket < 0) {
            if (m_running.load())
                logger().log_error("Accept failed");
            continue;
        }

        logger().log("Client connected");
        m_client_socket = client_socket;
        receive_lidar_msg_from_client(client_socket);
        if (m_client_socket.exchange(-1) == client_socket) {
            close(client_socket);
        }
        logger().log("Client disconnected");
    }
}