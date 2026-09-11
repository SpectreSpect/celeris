#include "sender_server.h"

template<typename Message>
SenderServer<Message>::SenderServer(
    std::string receiver_host, 
    uint16_t receiver_port)
    :   m_receiver_host(receiver_host),
        m_receiver_port(receiver_port){
}

template<typename Message>
SenderServer<Message>::~SenderServer() {
    stop();
}

template<typename Message>
void SenderServer<Message>::start() {
    std::lock_guard<std::mutex> lifecycle_lock(m_lifecycle_mtx);
    if (m_sender_thread.joinable())
        return;

    std::lock_guard<std::mutex> lock(m_pending_msg_mtx);
    m_is_stopping = false;
    m_sender_thread = std::thread(&SenderServer::sender_loop, this);
}

template<typename Message>
void SenderServer<Message>::stop() {
    std::lock_guard<std::mutex> lifecycle_lock(m_lifecycle_mtx);
    {
        std::lock_guard<std::mutex> lock(m_pending_msg_mtx);
        m_is_stopping = true;
        if (m_socket >= 0)
            shutdown(m_socket, SHUT_RDWR);
    }
    m_pending_msg_cv.notify_all();

    if (m_sender_thread.joinable())
        m_sender_thread.join();

    disconnect();
    std::lock_guard<std::mutex> lock(m_pending_msg_mtx);
    m_pending_msg.reset();
}

template<typename Message>
void SenderServer<Message>::submit(Message&& message) {
    {
        std::unique_lock<std::mutex> lock(m_pending_msg_mtx);
        if (m_is_stopping)
            return;
        m_pending_msg = std::make_unique<Message>(std::move(message));
    }

    m_pending_msg_cv.notify_one();
}

template<typename Message>
void SenderServer<Message>::disconnect() {
    std::lock_guard<std::mutex> lock(m_pending_msg_mtx);
    if (m_socket >= 0) {
        close(m_socket);
        m_socket = -1;
    }
}

template<typename Message>
bool SenderServer<Message>::ensure_connected() {
    LOG_METHOD();
    std::unique_lock<std::mutex> lock(m_pending_msg_mtx);
    if (m_is_stopping)
        return false;
    
    if (m_socket >= 0)
        return true;
    
    // Initiate connect under the lock so stop() cannot miss a new connection.
    m_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (m_socket < 0) {
        logger().log_error("socket() failed");
        return false;
    }

    int yes = 1;
    setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(m_receiver_port));

    if (inet_pton(AF_INET, m_receiver_host.c_str(), &address.sin_addr) <= 0) {
        logger().log_error() << "bad receiver_host: " << m_receiver_host << "\n";
        lock.unlock();
        disconnect();
        return false;
    }

    int result = connect(m_socket, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    const int connect_error = errno;
    lock.unlock();

    if (result < 0 && connect_error == EINPROGRESS) {
        for (;;) {
            {
                std::lock_guard<std::mutex> pending_lock(m_pending_msg_mtx);
                if (m_is_stopping)
                    return false;
            }
            pollfd connection{m_socket, POLLOUT, 0};
            result = poll(&connection, 1, 100);
            if (result == 0 || (result < 0 && errno == EINTR))
                continue;
            if (result > 0) {
                int error = 0;
                socklen_t error_size = sizeof(error);
                result = getsockopt(m_socket, SOL_SOCKET, SO_ERROR, &error, &error_size);
                if (error != 0)
                    result = -1;
            }
            break;
        }
    }

    if (result < 0) {
        logger().log() << 
            "Could not connect to receiver at " << 
            m_receiver_host << 
            ":" << 
            std::to_string(m_receiver_port) << "\n";
        // RCLCPP_WARN_THROTTLE(
        //     m_logger,
        //     *m_clock,
        //     2000,
        //     "ImuSender: Could not connect to IMU receiver at %s:%d",
        //     m_receiver_host.c_str(),
        //     m_receiver_port);
        disconnect();
        return false;
    }

    // Blocking sends are interrupted by stop() shutting down the socket.
    if (fcntl(m_socket, F_SETFL, 0) < 0) {
        disconnect();
        return false;
    }

    logger().log() << 
        "Connected to receiver at " << 
        m_receiver_host << ":" << 
        std::to_string(m_receiver_port) << "\n";

    // RCLCPP_INFO(
    //     m_logger, 
    //     "Connected to IMU receiver at %s:%d", 
    //     m_receiver_host.c_str(), 
    //     m_receiver_port
    // );

    return true;
}

template<typename Message>
bool SenderServer<Message>::send_all(const void* data, size_t size_bytes) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    size_t bytes_sent = 0;

    while (bytes_sent < size_bytes) {
        const ssize_t n = send(m_socket, bytes + bytes_sent, size_bytes - bytes_sent, MSG_NOSIGNAL);

        if (n <= 0) {
            disconnect();
            return false;
        }

        bytes_sent += static_cast<size_t>(n);
    }

    return true;
}

template<typename Message>
void SenderServer<Message>::send_msg(const Message& message) {
    if (!ensure_connected())
        return;

    if (!send_all(&message, sizeof(Message))) {
        logger().log("Failed to send a message");
        return;
    }
}

template<typename Message>
void SenderServer<Message>::sender_loop() {
    while (true) {
        std::unique_ptr<Message> message;
        {
            std::unique_lock<std::mutex> lock(m_pending_msg_mtx);
            m_pending_msg_cv.wait(lock, [this]() {
                return m_is_stopping || m_pending_msg != nullptr;
            });

            if (m_is_stopping)
                break;
            
            message = std::move(m_pending_msg);
            m_pending_msg.reset();
        }

        send_msg(*message);
    }
}
