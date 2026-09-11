#pragma once

#include <condition_variable>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cerrno>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <mutex>

#include "../../vulkan_self/logger/logger_header.h"

template<typename Message>
class SenderServer {
public:
    _XPARENT_NAME(SenderServer);

    SenderServer(
        std::string receiver_host, 
        uint16_t receiver_port
    );
    virtual ~SenderServer();

    void start();
    // Call from the owning thread, never the sender thread.
    void stop();
    void submit(Message&& message);

private:
    // rclcpp::Logger m_logger;
    // rclcpp::Clock::SharedPtr m_clock;
    // AxisMapping m_imu_axes;

    std::string m_receiver_host;
    uint16_t m_receiver_port;

    std::thread m_sender_thread;
    std::mutex m_lifecycle_mtx;
    std::mutex m_pending_msg_mtx;
    std::condition_variable m_pending_msg_cv;
    std::unique_ptr<Message> m_pending_msg = nullptr;
    bool m_is_stopping = false;

    int m_socket = -1;

    void disconnect();
    bool ensure_connected();
    // ImuMessage build_imu_message(sensor_msgs::msg::Imu::ConstSharedPtr imu_message);
    bool send_all(const void* data, size_t size_bytes);
    void send_msg(const Message& message);
    void sender_loop();
};

#include "sender_server.tpp"
