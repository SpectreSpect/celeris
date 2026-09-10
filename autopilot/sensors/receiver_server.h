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

#include "../../vulkan_self/logger/logger_header.h"

template<typename Message>
class ReceiverServer {
public:
    ReceiverServer(uint16_t port, size_t max_queued_messages);
    virtual ~ReceiverServer();

    void start();
    void stop();

    bool try_pop_front(Message& message);
    bool try_pop_back(Message& message);

private:
    uint16_t m_port = 5003;
    size_t m_max_queued_messages = 0;

    std::thread m_receiver_thread;
    std::mutex m_pending_msg_mtx;
    std::condition_variable m_pending_msg_cv;
    std::atomic<bool> m_running{false};

    int m_listen_socket = -1;
    std::atomic<int> m_client_socket{-1};

    std::deque<Message> m_msg_queue;
    std::mutex m_msg_queue_mtx;

    void close_listen_socket();
    bool read_exact(int socket, void* data, size_t byte_count);
    void push_back_message(Message& message);
    virtual bool read_exact_message(Message& message) = 0;
    bool receive_msg_from_client(int client_socket);
    void receiver_loop();
};

#include "receiver_server.tpp"