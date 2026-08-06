#pragma once

#include <condition_variable>
#include <cstdint>
#include <string>
#include <thread>
#include <chrono>
#include <deque>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "../../../vulkan_self/logger/logger_header.h"
#include "imu_measurement.h"

class ImuReceiver {
public:
    _XCLASS_NAME(ImuReceiver);

    // struct ImuMessage {
    //     glm::vec3 linear_acceleration;
    //     glm::vec3 angular_velocity;
    //     std::int64_t timestamp;
    // };

    ImuReceiver(
        uint16_t port = 5003,
        size_t max_queued_imu_messages = 1
    );

    void start();
    bool try_pop_back_imu_message(ImuMeasurement& message);
    
public:
    uint16_t m_port = 5003;
    size_t m_max_queued_imu_messages = 0;    

    std::thread m_receiver_thread;
    std::mutex m_pending_imu_mtx;
    std::condition_variable m_pending_imu_cv;
    std::atomic<bool> m_running{false};

    int m_listen_socket = -1;
    std::atomic<int> m_client_socket{-1};

    std::deque<ImuMeasurement> m_imu_message_queue;
    std::mutex m_imu_message_queue_mtx;

    void close_listen_socket();
    bool read_exact(int socket, void* data, size_t byte_count);
    void push_back_imu_message(ImuMeasurement& message);
    bool receive_imu_from_client(int client_socket);
    void receiver_loop();
};