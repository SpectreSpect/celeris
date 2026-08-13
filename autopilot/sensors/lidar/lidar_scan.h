#pragma once

#include "../../../vulkan_self/logger/logger_header.h"
#include "../../../renderer/point_cloud/point_cloud.h"
#include "../../../vulkan_self/vulkan_buffer.h"
#include "../../../renderer/scene_object.h"
#include "lidar_message.h"

class ManagerBundle;
class PointCloudPreprocessor;

class LidarScan : public SceneObject {
public:
    _XCLASS_NAME(LidarScan);

    LidarScan(
        ManagerBundle& manager_bundle, 
        PointCloudPreprocessor& point_cloud_preprocessor, 
        LidarMessage&& message
    );

    PointCloud point_cloud_from_lidar_msg(LidarMessage&& message);

    void save(std::filesystem::path path);

    PointCloud& point_cloud() noexcept;
    VulkanBuffer& normal_buffer() noexcept;
    uint64_t timestamp() const noexcept;

private:
    ManagerBundle* m_manager_bundle = nullptr;

    PointCloud m_point_cloud;
    VulkanBuffer m_normal_buffer;

    uint64_t m_timestamp = 0;
};