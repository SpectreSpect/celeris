#pragma once

#include "../../scene_object.h"
#include "../point_instance.h"
#include "../point_cloud.h"
#include "../../../path_utils.h"

#include <cstdint>
#include <vector>

class ManagerBundle;
class PointCloudPreprocessor;

class LidarScan : public SceneObject {
public:
    struct TimedPointSample {
        glm::vec3 p_local_ros{0.0f};
        float time = 0.0f;
        glm::vec3 base_pos_ros{0.0f};
        glm::vec3 base_rpy_ros{0.0f};
        bool valid = true;
    };

    struct FrameData {
        uint64_t timestamp_ns = 0;
        std::vector<TimedPointSample> samples;
        std::vector<PointInstance> points;
    };

    LidarScan(ManagerBundle& manager_bundle, 
              PointCloudPreprocessor& point_cloud_preprocessor, 
              const std::filesystem::path& path);
    LidarScan(ManagerBundle& manager_bundle, 
              PointCloudPreprocessor& point_cloud_preprocessor, 
              FrameData&& frame);

    void set_timestamp_ns(uint64_t timestamp_ns);
    uint64_t timestamp_ns() const noexcept;

    static glm::mat3 rpy_to_mat3_zyx(float roll, float pitch, float yaw);
    static glm::vec3 ros_pos_to_engine(const glm::vec3& p_ros);
    static void build_points_for_frame(FrameData& frame);

    PointCloud& point_cloud();
    VulkanBuffer& normal_buffer();

private:
    uint64_t m_timestamp_ns = 0;
    
    std::vector<PointInstance> m_points;
    std::vector<glm::vec4> m_normals;

    PointCloud m_point_cloud;
    VulkanBuffer m_normal_buffer;
    

    PointCloud load_from_file(ManagerBundle& manager_bundle, const std::filesystem::path& path);
    PointCloud load_from_frame(ManagerBundle& manager_bundle, FrameData&& frame);
    static FrameData read_frame_from_file(const std::filesystem::path& path);

    std::vector<glm::vec4> calculate_normals(std::vector<PointInstance> points);

    void remove_points_near_origin(std::vector<PointInstance>& points, std::vector<glm::vec4>& normals, float min_distance);
    void drop_out_points_and_normals(std::vector<PointInstance>& points, std::vector<glm::vec4>& normals, size_t target_size);
    void remove_invalid_points_and_normals(std::vector<PointInstance>& points, std::vector<glm::vec4>& normals);
    glm::vec3 triangle_normal(const PointInstance& a, const PointInstance& b, const PointInstance& c);
    bool is_point_valid(const PointInstance &p);
    int xy_id(int x, int y, int ring_width, int cloud_size);
    void get_normals(const std::vector<PointInstance>& points, std::vector<glm::vec4>& normals);
    bool is_same_object(const PointInstance &p0,const PointInstance &p1,
                        float rel_thresh = 1.5f, bool more_permissive_with_distance = true,
                        float abs_thresh = 0.12f);
    float radial_distance(const PointInstance &p);

    void keep_only_upward_facing_points_and_normals(std::vector<PointInstance>& points, std::vector<glm::vec4>& normals, float up_dot_threshold);

    void remove_ground_points_and_normals(std::vector<PointInstance>& points, std::vector<glm::vec4>& normals, float up_dot_threshold, float max_ground_height);
};
