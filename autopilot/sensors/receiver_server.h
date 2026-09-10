#pragma once

#include <condition_variable>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdint>
#include <thread>
#include <atomic>
#include <deque>
#include <mutex>

#include "../../vulkan_self/logger/logger_header.h"

template<typename Message>
class ReceiverServer {
public:
    _XPARENT_NAME(ReceiverServer);

    enum class QueueOverflowPolicy { DropOldest, DropNewest };

    ReceiverServer(
        uint16_t port,
        size_t max_queued_messages,
        QueueOverflowPolicy overflow_policy = QueueOverflowPolicy::DropOldest
    );
    virtual ~ReceiverServer();

    void start();
    // Call from each concrete child's destructor before its members are destroyed.
    // Must be called from the owning thread, never the receiver thread.
    void stop();

    bool try_pop_front(Message& message);
    bool try_pop_back(Message& message);

protected:
    uint16_t m_port = 5003;
    size_t m_max_queued_messages = 0;
    QueueOverflowPolicy m_overflow_policy = QueueOverflowPolicy::DropOldest;

    std::thread m_receiver_thread;
    std::mutex m_pending_msg_mtx;
    std::condition_variable m_pending_msg_cv;
    std::atomic<bool> m_running{false};

    std::mutex m_lifecycle_mtx;
    std::mutex m_socket_mtx;

    int m_listen_socket = -1;
    std::atomic<int> m_client_socket{-1};

    std::deque<Message> m_msg_queue;
    std::mutex m_msg_queue_mtx;

    void close_listen_socket(); // Requires m_socket_mtx to be locked.
    bool read_exact(int socket, void* data, size_t byte_count);
    void push_back_message(Message& message);
    virtual bool read_exact_message(int client_socket, Message& message) = 0;
    bool receive_msg_from_client(int client_socket);
    void receiver_loop();
};

#include "receiver_server.tpp"
