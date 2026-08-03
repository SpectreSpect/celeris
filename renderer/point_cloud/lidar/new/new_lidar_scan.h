#pragma once

#include "../../scene_object.h"
#include "../point_cloud.h"
#include "../../../../vulkan_self/vulkan_buffer.h"

class NewLidarScan : public SceneObject {
public:
    NewLidarScan();

private:
    PointCloud m_point_cloud;
    VulkanBuffer m_normal_buffer;
};