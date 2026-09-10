#include "receiver_server.h"

template<typename Message>
ReceiverServer<Message>::ReceiverServer(
    uint16_t port, 
    size_t max_queued_messages) 
    :   m_port(port),
        m_max_queued_messages(max_queued_messages){
}

template<typename Message>
ReceiverServer<Message>::~ReceiverServer() {
    stop();
}

template<typename Message>
void ReceiverServer<Message>::start() {
    std::lock_guard<std::mutex> lifecycle_lock(m_lifecycle_mtx);
    if (m_running.load())
        return;

    // A failed startup can leave a finished, but still joinable, thread.
    if (m_receiver_thread.joinable())
        m_receiver_thread.join();

    m_running = true;
    try {
        m_receiver_thread = std::thread(&ReceiverServer<Message>::receiver_loop, this);
    } catch (...) {
        m_running = false;
        throw;
    }
}

template<typename Message>
void ReceiverServer<Message>::stop() {
    std::lock_guard<std::mutex> lifecycle_lock(m_lifecycle_mtx);
    {
        std::lock_guard<std::mutex> socket_lock(m_socket_mtx);
        m_running = false;

        if (m_listen_socket >= 0)
            shutdown(m_listen_socket, SHUT_RDWR);
        const int client_socket = m_client_socket.load();
        if (client_socket >= 0)
            shutdown(client_socket, SHUT_RDWR);
    }
    m_pending_msg_cv.notify_all();

    if (m_receiver_thread.joinable())
        m_receiver_thread.join();

    std::lock_guard<std::mutex> socket_lock(m_socket_mtx);
    close_listen_socket();
}

template<typename Message>
bool ReceiverServer<Message>::try_pop_front(Message& message) {
    {
        std::unique_lock<std::mutex> lock(m_msg_queue_mtx);
        
        if (m_msg_queue.empty())
            return false;

        message = std::move(m_msg_queue.front());
        m_msg_queue.pop_front();
    }
    
    return true;
}

template<typename Message>
bool ReceiverServer<Message>::try_pop_back(Message& message) {
    {
        std::unique_lock<std::mutex> lock(m_msg_queue_mtx);

        if (m_msg_queue.empty())
            return false;

        message = std::move(m_msg_queue.back());
        m_msg_queue.pop_back();
    }

    return true;
}

template<typename Message>
void ReceiverServer<Message>::close_listen_socket() {
    if (m_listen_socket >= 0) {
        close(m_listen_socket);
        m_listen_socket = -1;
    }
}

template<typename Message>
bool ReceiverServer<Message>::read_exact(int socket, void* data, size_t byte_count) {
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

template<typename Message>
void ReceiverServer<Message>::push_back_message(Message& message) {
    {
        std::unique_lock<std::mutex> lock(m_msg_queue_mtx);

        if (m_msg_queue.size() == m_max_queued_messages)
            m_msg_queue.pop_front();
        m_msg_queue.push_back(message);
    }
}

template<typename Message>
bool ReceiverServer<Message>::receive_msg_from_client(int client_socket) {
    LOG_METHOD();

    while (m_running.load()) {
        Message message{};

        if (!read_exact_message(client_socket, message)) {
            return false;
        }

        push_back_message(message);
    }

    return true;
}

template<typename Message>
void ReceiverServer<Message>::receiver_loop() {
    LOG_METHOD();

    {
        // Publish a fully initialized listener before stop() can shut it down.
        std::lock_guard<std::mutex> socket_lock(m_socket_mtx);
        if (!m_running.load())
            return;

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
    }

    logger().log() << "Listening on port " << std::to_string(m_port) << "\n";

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

        {
            std::lock_guard<std::mutex> socket_lock(m_socket_mtx);
            if (!m_running.load()) {
                close(client_socket);
                break;
            }
            m_client_socket = client_socket;
        }
        logger().log("Client connected");
        receive_msg_from_client(client_socket);
        {
            std::lock_guard<std::mutex> socket_lock(m_socket_mtx);
            if (m_client_socket.exchange(-1) == client_socket)
                close(client_socket);
        }
        logger().log("Client disconnected");
    }
}
