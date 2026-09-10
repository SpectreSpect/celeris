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
#include "../receiver_server.h"
#include "lidar_message.h"

class PointCloudPreprocessor;
class ManagerBundle;
class LidarScan;

class NewLidarScanReceiver : public ReceiverServer<LidarMessage> {
public:
    _XCLASS_NAME(NewLidarScanReceiver);

    NewLidarScanReceiver(
        ManagerBundle& manager_bundle,
        PointCloudPreprocessor& point_cloud_preprocessor,
        uint16_t port = 5000,
        size_t max_queued_message = 3
    );

    ~NewLidarScanReceiver() override;

    std::unique_ptr<LidarScan> try_get_lidar_scan_from_lidar_msg(LidarMessage& message);
    std::unique_ptr<LidarScan> try_pop_front_lidar_scan();
    
public:
    ManagerBundle* m_manager_bundle = nullptr;
    PointCloudPreprocessor* m_point_cloud_preprocessor = nullptr;

    uint32_t m_max_points_per_message = 2'000'000;

    virtual bool read_exact_message(int client_socket, LidarMessage& message);
};
