#pragma once

#include "../vulkan_self/logger/logger_header.h"

class NewCelerisVisualizer;
class NewCeleris;
class Camera;

class NewCelerisUserController {
public:
    _XCLASS_NAME(NewCelerisUserController);

    struct NewCelerisUserControllerConfig {
        bool show_gazelle_next = true;
        bool show_voxel_grid = true;
    };

    NewCelerisUserController(
        NewCeleris& celeris, 
        NewCelerisVisualizer& celeris_visualizer, 
        const NewCelerisUserControllerConfig& config
    );

    void update();
    void display_celeris_general_panel(Camera& camera);
    void display_voxel_grid_panel();
    void display_interface(Camera& camera);

private:
    NewCeleris* m_celeris = nullptr;
    NewCelerisVisualizer* m_celeris_visualizer = nullptr;

    NewCelerisUserControllerConfig m_config;
};