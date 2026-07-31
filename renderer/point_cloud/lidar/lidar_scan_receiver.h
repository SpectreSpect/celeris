#pragma once

#include "lidar_scan.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <iostream>

#include "../../../vulkan_self/logger/logger_header.h"

class ManagerBundle;
class PointCloudPreprocessor;

class LidarScanReceiver {
public:
    _XCLASS_NAME(LidarScanReceiver);

    struct LidarScanMessageHeader {
        uint64_t timestamp_ns;
        uint32_t point_count;      
    };

    struct PointData
    {
        glm::vec3 position;
        float point_time;

        glm::vec3 linear_acceleration;
        glm::vec3 angular_velocity;
        glm::vec4 orientation;
    };

    explicit LidarScanReceiver(
        PointCloudPreprocessor& point_cloud_preprocessor,
        uint16_t port = 5000,
        size_t max_queued_frames = 3,
        uint32_t points_freq = 10
    );
    ~LidarScanReceiver();

    LidarScanReceiver(const LidarScanReceiver&) = delete;
    LidarScanReceiver& operator=(const LidarScanReceiver&) = delete;

    void start();
    void stop();

    void save_retrieved_scan(const void* data, size_t size_bytes, const std::filesystem::path& path);
    // void save_frame_data(const LidarScan::FrameData& frame_data, const std::filesystem::path& path);

    bool try_pop_frame(LidarScan::FrameData& frame);
    std::unique_ptr<LidarScan> try_pop_scan(ManagerBundle& manager_bundle);

    bool is_running() const noexcept;

private:
    uint16_t m_port = 0;
    size_t m_max_queued_frames = 0;
    uint32_t m_points_freq = 1;
    uint32_t m_ring_count = 16;
    PointCloudPreprocessor* m_point_cloud_preprocessor = nullptr;
    std::atomic<bool> m_running{false};
    int m_listen_socket = -1;
    std::atomic<int> m_client_socket{-1};
    std::thread m_thread;

    std::mutex m_queue_mutex;
    std::deque<LidarScan::FrameData> m_frames;

private:
    void receive_loop();
    bool receive_frames_from_client(int client_socket);
    bool read_exact(int socket, void* data, size_t byte_count);
    void push_frame(LidarScan::FrameData frame);
    void close_listen_socket();
};
