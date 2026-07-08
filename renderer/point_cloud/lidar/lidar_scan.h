#pragma once

#include "../../scene_object.h"
#include "../point_instance.h"
#include "../point_cloud.h"
#include "../../../path_utils.h"
#include "../../../vulkan_self/logger/logger_header.h"

#include <cstdint>
#include <vector>

class ManagerBundle;
class PointCloudPreprocessor;

class LidarScan : public SceneObject {
public:
    _XCHILD_NAME(LidarScan);

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
        std::vector<PointInstance> points;

        FrameData() = default;
        FrameData(const std::filesystem::path& path) {
            load(path);
        }

        void save(const std::filesystem::path& path) {
            std::ofstream out(path.string());

            uint32_t sample_count = samples.size();
            uint32_t point_count = points.size();

            out.write((const char*)&timestamp_ns, sizeof(timestamp_ns));
            out.write((const char*)&ring_count, sizeof(ring_count));
            out.write((const char*)&sample_count, sizeof(sample_count));
            out.write((const char*)&point_count, sizeof(point_count));
            out.write((const char*)samples.data(), samples.size() * sizeof(LidarScan::TimedPointSample));
            out.write((const char*)points.data(), points.size() * sizeof(PointInstance));

            out.close();
        }

        void load(const std::filesystem::path& path) {
            std::ifstream in(path.string(), std::ios::binary);
            
            if (!in) throw std::runtime_error("Failed to open: " + path.string());

            // in.read((const char*)&timestamp_ns, sizeof(timestamp_ns));

            uint32_t sample_count = 0;
            uint32_t point_count = 0;

            in.read(reinterpret_cast<char*>(&timestamp_ns), sizeof(timestamp_ns));
            in.read(reinterpret_cast<char*>(&ring_count), sizeof(ring_count));

            in.read(reinterpret_cast<char*>(&sample_count), sizeof(sample_count));
            in.read(reinterpret_cast<char*>(&point_count), sizeof(point_count));

            samples.resize(sample_count);
            points.resize(point_count);

            // point_count = 64 * 5;

            // for (int i = 1; i < point_count; i++) {
            //     float result = point_count / (float)i;
                
            //     if ((int)result == result)
            //         std::cout << i << ": " << result << std::endl;
            // }

            in.read(reinterpret_cast<char*>(samples.data()), sample_count * sizeof(LidarScan::TimedPointSample));
            in.read(reinterpret_cast<char*>(points.data()), point_count * sizeof(PointInstance));

            for (int i = 0; i < points.size(); i++) {
                float t = (float)i / (float)(points.size() - 1);

                // float dist = glm::length(points[i].position);

                // if (dist < 4.0)
                //     std::cout << i << ": " << dist << std::endl;

                if (i < 1399)
                    points[i].color = glm::vec4(0, 1, 0, 1);
                else
                    points[i].color *= t;
            }
            
            in.close();

            // out.close();
        }
    };

    LidarScan(
        ManagerBundle& manager_bundle, 
        PointCloudPreprocessor& point_cloud_preprocessor, 
        const std::filesystem::path& path
    );
    LidarScan(
        ManagerBundle& manager_bundle, 
        const std::filesystem::path& path
    );
    LidarScan(
        ManagerBundle& manager_bundle, 
        PointCloudPreprocessor& point_cloud_preprocessor, 
        FrameData&& frame
    );

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

    // bool is_point_valid(const PointInstance &p);
    // glm::vec3 triangle_normal(const PointInstance& a, const PointInstance& b, const PointInstance& c);
    // int xy_id(int x, int y, int ring_width, int cloud_size);
    // bool is_same_object(
    //     const PointInstance &p0,
    //     const PointInstance &p1,
    //     float rel_thresh = 1.5f,
    //     bool more_permissive_with_distance = true,
    //     float abs_thresh = 0.12f
    // );

    // void get_normals(const std::vector<PointInstance>& points, std::vector<glm::vec4>& normals);
    // void remove_invalid_points_and_normals(std::vector<PointInstance>& points, std::vector<glm::vec4>& normals);
    // void drop_out_points_and_normals(std::vector<PointInstance>& points, std::vector<glm::vec4>& normals, size_t target_size);
    // void remove_points_near_origin(std::vector<PointInstance>& points, std::vector<glm::vec4>& normals, float min_distance);

    // std::vector<glm::vec4> calculate_normals(std::vector<PointInstance> points);
    // float radial_distance(const PointInstance &p);
    // void keep_only_upward_facing_points_and_normals(std::vector<PointInstance>& points, std::vector<glm::vec4>& normals, float up_dot_threshold);
    // void remove_ground_points_and_normals(std::vector<PointInstance>& points, std::vector<glm::vec4>& normals, float up_dot_threshold, float max_ground_height);
};
