#pragma once

#include "../vulkan_self/logger/logger_header.h"
#include "../camera/camera.h"
#include "celeris.h"
#include "celeris_visualizer.h"
#include "../camera/controllers/fps_camera_controller.h"

class CelerisVisualizer;
class KeyboardInputReciever;
class GamepadController;

class CelerisUserController {
public:
    _XCLASS_NAME(CelerisUserController);

    enum class CameraControllerMode {
        FPS,
        ThirdPerson
    };

    CelerisUserController(celeris::Celeris& celeris, CelerisVisualizer& celeris_visualizer);

    void update(
        float delta_time, 
        Camera& camera, 
        KeyboardInputReciever& keyboard_input_reciever, 
        FPSCameraController& fps_camera_controller
    );
    void display_interface(Camera& camera, GamepadController& gamepad_controller);

    inline void use_third_person_camera_controller() {
        LOG_METHOD();
        
        m_camera_controller_mode = CameraControllerMode::ThirdPerson;
    };

    // float car_speed = celeris.car_speed();

    inline void start_path_planning() {  
        LOG_METHOD();

        logger().check(m_celeris, "Celeris was null");

        const bool already_has_path = m_celeris->has_planned_path();
        m_has_planned_path = m_celeris->request_path_replan();
        if (m_has_planned_path || already_has_path)
            m_celeris->reset_local_planner_tracking();
    };

    inline bool make_pose_from_camera(Camera& camera, NonholonomicPos& out_pose) {
        LOG_METHOD();

        glm::vec3 horizontal_front(camera.front.x, 0.0f, camera.front.z);
        if (glm::dot(horizontal_front, horizontal_front) == 0.0f)
            return false;

        out_pose.pos = camera.position;
        out_pose.theta = std::atan2(horizontal_front.z, horizontal_front.x);

        glm::vec3 grounded_position = out_pose.pos;
        if (m_celeris->adjust_to_ground(grounded_position))
            out_pose.pos = grounded_position;

        return true;
    };

    inline void place_start(Camera& camera) {
        LOG_METHOD();

        logger().check(m_celeris, "Celeris was null");

        // std::cout << "Place start" << std::endl;
        NonholonomicPos pose;
        if (make_pose_from_camera(camera, pose)) {
            m_celeris->set_start(pose);
            m_has_start_pos = true;
            m_has_planned_path = false;
        }
    };

    inline void place_end(Camera& camera) {
        LOG_METHOD();

        logger().check(m_celeris, "Celeris was null");

        NonholonomicPos pose;
        if (make_pose_from_camera(camera, pose)) {
            m_celeris->set_goal(pose);
            m_has_end_pos = true;
            m_has_planned_path = false;
        }
    };

    inline void move_start_to_vehicle() {
        LOG_METHOD();

        logger().check(m_celeris, "Celeris was null");

        m_celeris->set_start(NonholonomicPos::from_transform(m_celeris->vehicle_transform()));
        m_has_start_pos = true;
        m_has_planned_path = false;
    };

    inline void add_directional_waypoint(Camera& camera) {
        LOG_METHOD();

        logger().check(m_celeris, "Celeris was null");
        
        NonholonomicPos pose;
        if (make_pose_from_camera(camera, pose)) {
            m_celeris->add_waypoint(pose);
        }
    };

    inline void add_nondirectional_waypoint(Camera& camera) {
        LOG_METHOD();

        logger().check(m_celeris, "Celeris was null");

        glm::vec3 position = camera.position;
        if (m_celeris->adjust_to_ground(position)) {
            m_celeris->add_waypoint(position);
        }
    };

    inline void delete_last_waypoint() {
        LOG_METHOD();

        logger().check(m_celeris, "Celeris was null");

        m_celeris->delete_last_waypoint();
    };

    inline uint32_t pack_inflated_voxel_color(const float color[4]) {
        auto pack_channel = [](float value) -> uint32_t {
            return static_cast<uint32_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
        };

        return (pack_channel(color[0]) << 24u) |
               (pack_channel(color[1]) << 16u) |
               (pack_channel(color[2]) << 8u) |
               pack_channel(color[3]);
    };

    inline float path_length(const std::vector<NonholonomicPos>& path) {
        float length = 0.0f;
        for (size_t i = 1; i < path.size(); i++) {
            length += glm::distance(path[i - 1].pos, path[i].pos);
        }

        return length;
    };

    inline void start_car_playback() {
        LOG_METHOD();

        logger().check(m_celeris, "Celeris was null");

        m_car_playback_path = m_celeris->path_result_snapshot().nonholonomic_astar_path;
        m_car_playback_total_length = path_length(m_car_playback_path);
        m_car_playback_distance = 0.0f;
        m_car_playback_active = m_car_playback_path.size() >= 2 && m_car_playback_total_length > 0.0f;

        if (m_car_playback_active) {
            m_celeris_visualizer->set_car_pose_override(m_car_playback_path.front());
        }
    };

    inline NonholonomicPos sample_path_pose(const std::vector<NonholonomicPos>& path, float target_distance) {
        if (path.empty())
            return NonholonomicPos{};

        if (path.size() == 1)
            return path.front();

        float traversed_distance = 0.0f;
        for (size_t i = 1; i < path.size(); i++) {
            const NonholonomicPos& from = path[i - 1];
            const NonholonomicPos& to = path[i];
            const float segment_length = glm::distance(from.pos, to.pos);

            if (segment_length <= 0.0f)
                continue;

            if (traversed_distance + segment_length >= target_distance) {
                const float t = glm::clamp(
                    (target_distance - traversed_distance) / segment_length,
                    0.0f,
                    1.0f
                );

                NonholonomicPos pose = from;
                pose.pos = glm::mix(from.pos, to.pos, t);
                pose.theta = from.theta + NonholonomicAStar::angle_diff(from.theta, to.theta) * t;
                pose.steer = to.steer;
                pose.dir = to.dir;
                pose.dubins_segment_id = to.dubins_segment_id;
                return pose;
            }

            traversed_distance += segment_length;
        }

        return path.back();
    };

    inline void use_fps_camera_controller(FPSCameraController& fps_camera_controller) {
        if (m_camera_controller_mode == CameraControllerMode::FPS)
            return;

        const glm::vec3 front = glm::normalize(fps_camera_controller.camera().front);
        fps_camera_controller.yaw = glm::degrees(std::atan2(front.z, front.x));
        fps_camera_controller.pitch = glm::degrees(std::asin(glm::clamp(front.y, -1.0f, 1.0f)));
        fps_camera_controller.first_mouse = true;
        m_camera_controller_mode = CameraControllerMode::FPS;
    };

    inline CameraControllerMode camera_controller_mode() const noexcept {
        return m_camera_controller_mode;
    }

    bool show_voxel_grid() {
        return m_show_voxel_grid;
    }

private:
    celeris::Celeris* m_celeris = nullptr;
    CelerisVisualizer* m_celeris_visualizer = nullptr;

    CameraControllerMode m_camera_controller_mode = CameraControllerMode::FPS;
    
    bool m_has_start_pos = false;
    bool m_has_end_pos = false;
    bool m_has_planned_path = false;

    bool m_show_voxel_grid = true;
    bool m_display_inflated_voxels = false;
    float m_inflated_voxel_color[4];
    float m_inflated_curvature_limit_exceeded_voxel_color[4];

    int m_negative_x_inflation_size = 0;
    int m_positive_x_inflation_size = 0;
    int m_negative_y_inflation_size = 0;
    int m_positive_y_inflation_size = 0;
    int m_negative_z_inflation_size = 0;
    int m_positive_z_inflation_size = 0;

    char m_map_save_file_name[128] = "map";
    std::string m_map_save_status;
    bool m_map_save_failed = false;
    std::string m_map_localization_status;
    bool m_map_localization_failed = false;
    char m_waypoint_path_file_name[128] = "path";
    std::string m_waypoint_path_status;
    bool m_waypoint_path_failed = false;

    const std::filesystem::path m_saved_maps_directory = std::filesystem::path("saved_maps");

    const std::filesystem::path m_saved_waypoint_paths_directory =
        std::filesystem::path("saved_waypoint_paths");

    std::vector<NonholonomicPos> m_car_playback_path;
    bool m_car_playback_active = false;
    bool m_car_playback_loop = false;
    float m_car_playback_distance = 0.0f;
    float m_car_playback_total_length = 0.0f;
    float m_car_playback_speed = 4.0f;
};