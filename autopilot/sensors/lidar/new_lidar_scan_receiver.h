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
#include "lidar_message.h"

class PointCloudPreprocessor;
class ManagerBundle;
class NewLidarScan;

class NewLidarScanReceiver {
public:
    _XCLASS_NAME(NewLidarScanReceiver);

    // struct ImuMessage {
    //     glm::vec3 linear_acceleration;
    //     glm::vec3 angular_velocity;
    //     std::int64_t timestamp;
    // };

    NewLidarScanReceiver(
        ManagerBundle& manager_bundle,
        PointCloudPreprocessor& point_cloud_preprocessor,
        uint16_t port = 5000,
        size_t max_queued_message = 3
    );

    void start();
    bool try_pop_front_lidar_msg(LidarMessage& message);
    std::unique_ptr<NewLidarScan> try_pop_front_lidar_scan();
    
public:
    ManagerBundle* m_manager_bundle = nullptr;
    PointCloudPreprocessor* m_point_cloud_preprocessor = nullptr;

    uint16_t m_port = 5003;
    size_t m_max_queued_messages = 0;

    uint32_t m_max_points_per_message = 2'000'000;

    std::thread m_receiver_thread;
    std::mutex m_pending_lidar_msg_mtx;
    std::condition_variable m_pending_lidar_msg_cv;
    std::atomic<bool> m_running{false};

    int m_listen_socket = -1;
    std::atomic<int> m_client_socket{-1};

    std::deque<LidarMessage> m_lidar_msg_queue;
    std::mutex m_lidar_msg_queue_mtx;

    void close_listen_socket();
    bool read_exact(int socket, void* data, size_t byte_count);
    void push_back_lidar_msg(LidarMessage& message);
    bool receive_lidar_msg_from_client(int client_socket);
    void receiver_loop();
};