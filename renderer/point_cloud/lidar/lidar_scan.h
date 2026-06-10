#pragma once

#include "../../scene_object.h"
#include "../point_cloud.h"
#include "../../../path_utils.h"
#include "../../../vulkan_self/logger/logger_header.h"

class ManagerBundle;

class LidarScan : public SceneObject {
public:
    _XCHILD_NAME(LidarScan);
    
    LidarScan(ManagerBundle& manager_bundle, const std::filesystem::path& path);

    void set_timestamp_ns(uint32_t timestamp_ns);

    static glm::mat3 rpy_to_mat3_zyx(float roll, float pitch, float yaw);
    static glm::vec3 ros_pos_to_engine(const glm::vec3& p_ros);

    PointCloud& point_cloud();
    VulkanBuffer& normal_buffer();

    static LidarScan from_preprocess(ManagerBundle& manager_bundle, const std::filesystem::path& path);

private:
    uint64_t m_timestamp_ns = 0;
    
    std::vector<PointInstance> m_points;
    std::vector<glm::vec4> m_normals;

    PointCloud m_point_cloud;
    VulkanBuffer m_normal_buffer;
    
    PointCloud load_from_file(ManagerBundle& manager_bundle, const std::filesystem::path& path);

    bool is_point_valid(const PointInstance &p);
    glm::vec3 triangle_normal(const PointInstance& a, const PointInstance& b, const PointInstance& c);
    int xy_id(int x, int y, int ring_width, int cloud_size);
    bool is_same_object(
        const PointInstance &p0,
        const PointInstance &p1,
        float rel_thresh = 1.5f,
        bool more_permissive_with_distance = true,
        float abs_thresh = 0.12f
    );

    void get_normals(const std::vector<PointInstance>& points, std::vector<glm::vec4>& normals);
    void remove_invalid_points_and_normals(std::vector<PointInstance>& points, std::vector<glm::vec4>& normals);
    void drop_out_points_and_normals(std::vector<PointInstance>& points, std::vector<glm::vec4>& normals, size_t target_size);
    void remove_points_near_origin(std::vector<PointInstance>& points, std::vector<glm::vec4>& normals, float min_distance);
    void to_preprocess_points();

    std::vector<glm::vec4> calculate_normals(std::vector<PointInstance> points);
    float radial_distance(const PointInstance &p);
    void keep_only_upward_facing_points_and_normals(std::vector<PointInstance>& points, std::vector<glm::vec4>& normals, float up_dot_threshold);
    void remove_ground_points_and_normals(std::vector<PointInstance>& points, std::vector<glm::vec4>& normals, float up_dot_threshold, float max_ground_height);
};