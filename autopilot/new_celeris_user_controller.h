#pragma once

#include "../vulkan_self/logger/logger_header.h"

class KeyboardInputReciever;
class NewCelerisVisualizer;
class NonholonomicPos;
class NewCeleris;
class Camera;

class NewCelerisUserController {
public:
    _XCLASS_NAME(NewCelerisUserController);

    struct NewCelerisUserControllerConfig {
        bool show_gazelle_next = true;
        bool show_voxel_grid = true;
        
        bool show_guide_path = true;
    };

    NewCelerisUserController(
        NewCeleris& celeris, 
        NewCelerisVisualizer& celeris_visualizer, 
        const NewCelerisUserControllerConfig& config
    );

    void update(const Camera& camera, KeyboardInputReciever& keyboard_input_reciever);
    void display_celeris_general_panel(Camera& camera);
    void display_voxel_grid_panel();
    void display_path_planner_panel(Camera& camera);
    void display_interface(Camera& camera);

private:
    NewCeleris* m_celeris = nullptr;
    NewCelerisVisualizer* m_celeris_visualizer = nullptr;

    NewCelerisUserControllerConfig m_config;

    bool make_pose_from_camera(const Camera& camera, NonholonomicPos& out_pose);
    void place_planner_start(const Camera& camera);
    void place_planner_goal(const Camera& camera);
    void replan_path();

    // inline void place_start(Camera& camera) {
    //     LOG_METHOD();

    //     logger().check(m_celeris, "Celeris was null");

    //     // std::cout << "Place start" << std::endl;
    //     NonholonomicPos pose;
    //     if (make_pose_from_camera(camera, pose)) {
    //         m_celeris->set_start(pose);
    //         m_has_start_pos = true;
    //         m_has_planned_path = false;
    //     }
    // };
};