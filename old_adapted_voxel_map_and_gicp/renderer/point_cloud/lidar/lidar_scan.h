#pragma once

#include "../../../../renderer/scene_object.h"
#include "../point_instance.h"
#include "../point_cloud.h"
#include "../../../../path_utils.h"
#include "../../../../vulkan_self/logger/logger_header.h"

#include <cstdint>
#include <vector>

class ManagerBundle;
class PointCloudPreprocessor;

class OldLidarScan : public SceneObject {
public:
    _XCHILD_NAME(OldLidarScan);

    struct TimedPointSample {
        glm::vec3 p_local_ros{0.0f};
        float time = 0.0f;
        glm::vec3 base_pos_ros{0.0f};
        glm::vec3 base_rpy_ros{0.0f};
        bool valid = true;
    };

    struct FrameData {
        uint64_t timestamp_ns = 0;
        uint32_t ring_count = 0;
        std::vector<TimedPointSample> samples;
        std::vector<OldPointInstance> points;
    };

    OldLidarScan(
        ManagerBundle& manager_bundle, 
        PointCloudPreprocessor& point_cloud_preprocessor, 
        const std::filesystem::path& path
    );
    OldLidarScan(
        ManagerBundle& manager_bundle, 
        const std::filesystem::path& path
    );
    OldLidarScan(
        ManagerBundle& manager_bundle, 
        PointCloudPreprocessor& point_cloud_preprocessor, 
        FrameData&& frame
    );

    void set_timestamp_ns(uint64_t timestamp_ns);
    uint64_t timestamp_ns() const noexcept;

    static glm::mat3 rpy_to_mat3_zyx(float roll, float pitch, float yaw);
    static glm::vec3 ros_pos_to_engine(const glm::vec3& p_ros);
    static void build_points_for_frame(FrameData& frame);

    OldPointCloud& point_cloud();
    VulkanBuffer& normal_buffer();

private:
    uint64_t m_timestamp_ns = 0;
    
    std::vector<OldPointInstance> m_points;
    std::vector<glm::vec4> m_normals;

    OldPointCloud m_point_cloud;
    VulkanBuffer m_normal_buffer;
    
    OldPointCloud load_from_file(ManagerBundle& manager_bundle, const std::filesystem::path& path);
    OldPointCloud load_from_frame(ManagerBundle& manager_bundle, FrameData&& frame);
    static FrameData read_frame_from_file(const std::filesystem::path& path);
};
