#pragma once

#include "lidar_scan.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

class ManagerBundle;
class PointCloudPreprocessor;

class LidarScanReceiver {
public:
    explicit LidarScanReceiver(PointCloudPreprocessor& point_cloud_preprocessor, 
                               uint16_t port = 5000, size_t max_queued_frames = 3);
    ~LidarScanReceiver();

    LidarScanReceiver(const LidarScanReceiver&) = delete;
    LidarScanReceiver& operator=(const LidarScanReceiver&) = delete;

    void start();
    void stop();

    bool try_pop_frame(LidarScan::FrameData& frame);
    std::unique_ptr<LidarScan> try_pop_scan(ManagerBundle& manager_bundle);

    bool is_running() const noexcept;

private:
    uint16_t m_port = 0;
    size_t m_max_queued_frames = 0;
    PointCloudPreprocessor* m_point_cloud_preprocessor = nullptr;
    std::atomic<bool> m_running{false};
    int m_listen_socket = -1;
    std::atomic<int> m_client_socket{-1};
    std::thread m_thread;

    std::mutex m_queue_mutex;
    std::deque<LidarScan::FrameData> m_frames;

    void receive_loop();
    bool receive_frames_from_client(int client_socket);
    bool read_exact(int socket, void* data, size_t byte_count);
    void push_frame(LidarScan::FrameData frame);
    void close_listen_socket();
};
