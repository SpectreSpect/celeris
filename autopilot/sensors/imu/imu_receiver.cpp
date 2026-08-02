#include "imu_receiver.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

ImuReceiver::ImuReceiver(uint16_t port, size_t max_queued_imu_messages)
    :   m_port(port),
        m_max_queued_imu_messages(max_queued_imu_messages) {}

void ImuReceiver::start() {
    if (m_running.exchange(true))
        return;

    m_receiver_thread = std::thread(&ImuReceiver::receiver_loop, this);
}

bool ImuReceiver::try_pop_back_imu_message(ImuMessage& message) {
    {
        std::unique_lock<std::mutex> lock(m_imu_message_queue_mtx);

        if (m_imu_message_queue.empty())
            return false;

        message = std::move(m_imu_message_queue.back());
        m_imu_message_queue.pop_back();
    }

    return true;
}

void ImuReceiver::close_listen_socket() {
    if (m_listen_socket > 0) {
        close(m_listen_socket);
        m_listen_socket = -1;
    }
}

bool ImuReceiver::read_exact(int socket, void* data, size_t byte_count) {
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

void ImuReceiver::push_back_imu_message(ImuMessage& message) {
    {
        std::unique_lock<std::mutex> lock(m_imu_message_queue_mtx);

        if (m_imu_message_queue.size() == m_max_queued_imu_messages)
            m_imu_message_queue.pop_back();
        m_imu_message_queue.push_back(message);
    }
}

bool ImuReceiver::receive_imu_from_client(int client_socket) {
    LOG_METHOD();

    while (m_running.load()) {
        ImuMessage imu_message{};

        if (!read_exact(client_socket, &imu_message, sizeof(ImuMessage)))
            return false;

        push_back_imu_message(imu_message);
    }

    return true;
}

void ImuReceiver::receiver_loop() {
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

    logger().log() << "ImuReceiver: Listening on port " << std::to_string(m_port) << "\n";

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
        receive_imu_from_client(client_socket);
        if (m_client_socket.exchange(-1) == client_socket) {
            close(client_socket);
        }
        logger().log("Client disconnected");
    }
}