#pragma once

#include <vector>


#include "../../scene_object.h"
#include "../../../path_utils.h"
#include "../../manager_bundle.h"
#include "../../../vulkan_self/logger/logger.h"

#include "lidar_scan.h"

class LidarVideo : public SceneObject {
public:
    _XCHILD_NAME(LidarVideo);

    struct IndexEntry {
        uint64_t frame_id;
        uint64_t timestamp_ns;
        std::string filename;
        uint32_t point_count;
        glm::vec3 position{0.0f};
        glm::vec3 rotation_rpy{0.0f}; // roll,pitch,yaw in ROS
        glm::vec3 angular_velocity;
        glm::vec3 linear_acceleration;
    };

    LidarVideo(ManagerBundle& manager_bundle, const std::filesystem::path& csv_path, int first_frame = -1, int last_frame = -1);

    void load_from_file(ManagerBundle& manager_bundle, const std::filesystem::path& csv_path, int first_frame = -1, int last_frame = -1);

    static bool parse_u64(const std::string& s, uint64_t& out);
    static bool parse_u32(const std::string& s, uint32_t& out);
    static bool parse_f(const std::string& s, float& out);

    static inline glm::vec3 ros_vec_to_engine(const glm::vec3& v_ros);
    static glm::vec3 mat3_to_euler_xyz_custom(const glm::mat3& R);
    static void mat4_to_pose(const glm::mat4& M, glm::vec3& position, glm::vec3& rotation);
    static glm::mat4 pose_to_mat4(const glm::vec3& position, const glm::vec3& rotation);
    static inline glm::vec3 mat3_to_rpy_zyx(const glm::mat3& R);
    static inline glm::mat3 rpy_to_mat3(float roll, float pitch, float yaw);
    static inline glm::mat3 basis_M_ros_to_engine();
    static glm::vec3 ros_rpy_to_engine_rpy(const glm::vec3& rpy_ros);

    uint32_t current_frame_id() const;
    void next_frame();
    void previous_frame();
    void set_frame(uint32_t id);

    size_t get_frame_id(float time, size_t search_start_id = 0) const;

    void move(float time);
    void pause();
    void resume();
    void update(float delta_time);

    void set_looped(bool looped);
    bool is_paused() const;

private:
    uint32_t m_current_frame_id = 0;
    std::vector<LidarScan> m_scans;

    float m_timer = 0.0f;
    bool m_paused = false;
    bool m_looped = true;

    std::vector<uint64_t> m_frame_timestamps_ns;

    float frame_time_seconds(size_t frame_id) const;

    void sync();
};