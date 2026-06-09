#pragma once
#include "point_cloud_frame.h"
#include <algorithm>

struct IndexEntry {
    uint64_t frame_id;
    uint64_t timestamp_ns;
    std::string filename;
    uint32_t point_count;
    glm::vec3 position{0.0f};
    glm::vec3 rotation_rpy{0.0f}; // roll,pitch,yaw in ROS
};

class PointCloudVideo : public Drawable, public Transformable {
public:
    std::vector<PointCloudFrame> frames;
    size_t current_frame = 0;
    size_t last_frame_id = 0;
    float timer = 0.0f;
    bool paused = false;
    bool looped = true;

    PointCloudVideo() = default;
    void load_from_file(const std::filesystem::path& csv_path, int max_frames = -1);
    size_t get_frame_id(float time, size_t search_start_id = 0);
    void move(float time = 0.0f);
    void pause();
    void resume();
    void update(float delta_time);
    virtual void draw(RenderState state) override;



    static inline glm::vec3 ros_pos_to_engine(const glm::vec3& p_ros) {
        // same as your point remap: (-x, z, y)
        return glm::vec3(-p_ros.x, p_ros.z, p_ros.y);
    }

    static inline glm::mat3 basis_M_ros_to_engine() {
        // Columns are images of ROS basis vectors expressed in engine coords
        // col0 = M*[1,0,0] = (-1,0,0)
        // col1 = M*[0,1,0] = (0,0,1)
        // col2 = M*[0,0,1] = (0,1,0)
        glm::mat3 M(1.0f);
        M[0] = glm::vec3(-1, 0, 0);
        M[1] = glm::vec3( 0, 0, 1);
        M[2] = glm::vec3( 0, 1, 0);
        return M;
    }

    // Build R = Rz(yaw) * Ry(pitch) * Rx(roll)  (same order as your Transformable)
    static inline glm::mat3 rpy_to_mat3(float roll, float pitch, float yaw) {
        glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), roll,  glm::vec3(1,0,0));
        glm::mat4 Ry = glm::rotate(glm::mat4(1.0f), pitch, glm::vec3(0,1,0));
        glm::mat4 Rz = glm::rotate(glm::mat4(1.0f), yaw,   glm::vec3(0,0,1));
        return glm::mat3(Rz * Ry * Rx);
    }

    // Extract roll/pitch/yaw from R = Rz(yaw) * Ry(pitch) * Rx(roll)
    static inline glm::vec3 mat3_to_rpy_zyx(const glm::mat3& R) {
        // Using the standard ZYX (yaw-pitch-roll) extraction
        // Beware GLM indexing: R[col][row]
        float r20 = R[0][2]; // row2 col0
        float r10 = R[0][1]; // row1 col0
        float r00 = R[0][0]; // row0 col0
        float r21 = R[1][2]; // row2 col1
        float r22 = R[2][2]; // row2 col2

        float pitch = std::asin(glm::clamp(-r20, -1.0f, 1.0f));
        float yaw   = std::atan2(r10, r00);
        float roll  = std::atan2(r21, r22);

        return glm::vec3(roll, pitch, yaw);
    }

    static inline glm::vec3 ros_rpy_to_engine_rpy(const glm::vec3& rpy_ros) {
        glm::mat3 R_ros = rpy_to_mat3(rpy_ros.x, rpy_ros.y, rpy_ros.z);

        glm::mat3 M = basis_M_ros_to_engine();
        glm::mat3 R_eng = M * R_ros * glm::transpose(M); // change of basis

        return mat3_to_rpy_zyx(R_eng);
    }
};