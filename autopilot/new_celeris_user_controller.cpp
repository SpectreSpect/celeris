#include "new_celeris_user_controller.h"

#include "../vulkan_self/keyboard_input_reciever.h"
#include "../voxel_grid_vulkan/voxel_grid.h"
#include "../a_star/a_star_structures.h"
#include "new_celeris_visualizer.h"
#include "../camera/camera.h"
#include "../imgui_layer.h"
#include "new_celeris.h"

NewCelerisUserController::NewCelerisUserController(
    NewCeleris& celeris, 
    NewCelerisVisualizer& celeris_visualizer,
    const NewCelerisUserControllerConfig& config) 
    :   m_celeris(&celeris),
        m_celeris_visualizer(&celeris_visualizer),
        m_config(config) {
}

void NewCelerisUserController::update(const Camera& camera, KeyboardInputReciever& keyboard_input_reciever) {
    m_celeris_visualizer->gazelle_next_visible(m_config.show_gazelle_next);
    m_celeris_visualizer->voxel_grid_visible(m_config.show_voxel_grid);

    if (keyboard_input_reciever.on_key_pressed(GLFW_KEY_1))
        place_planner_start(camera);
    if (keyboard_input_reciever.on_key_pressed(GLFW_KEY_2))
        place_planner_goal(camera);
}

void NewCelerisUserController::display_celeris_general_panel(Camera& camera) {
    ImGui::Begin("Celeris general");

    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

    ImGui::TextUnformatted("Camera position:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "x: %.2f", camera.position.x);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "y: %.2f", camera.position.y);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 0.35f, 1.0f, 1.0f), "z: %.2f", camera.position.z);

    ImGui::Checkbox("Show gazelle next", &m_config.show_gazelle_next);

    ImGui::End();
}

void NewCelerisUserController::display_voxel_grid_panel() {
    ImGui::Begin("Voxel grid");

    ImGui::Checkbox("Show voxel grid", &m_config.show_voxel_grid);

    ImGui::End();
}

void NewCelerisUserController::display_path_planner_panel(Camera& camera){
    ImGui::Begin("Path planner");

    if (ImGui::Button("Place start"))
        place_planner_start(camera);
    ImGui::SameLine();
    ImGui::TextUnformatted("Key: 1");

    if (ImGui::Button("Place goal"))
        place_planner_goal(camera);
    ImGui::SameLine();
    ImGui::TextUnformatted("Key: 2");

    ImGui::End();
}

void NewCelerisUserController::display_interface(Camera& camera) {
    display_celeris_general_panel(camera);
    display_voxel_grid_panel();
    display_path_planner_panel(camera);
}

bool NewCelerisUserController::make_pose_from_camera(const Camera& camera, NonholonomicPos& out_pose) {
    LOG_METHOD();

    logger().check(m_celeris, "Celeris was null");

    if (!NonholonomicPos::from_camera(camera, out_pose))
        return false;
    
    glm::vec3 grounded_position = out_pose.pos;
    if (m_celeris->adjust_to_ground(grounded_position))
        out_pose.pos = grounded_position;
    
    return true;
}

void NewCelerisUserController::place_planner_start(const Camera& camera) {
    LOG_METHOD();

    NonholonomicPos pose;
    if (!make_pose_from_camera(camera, pose))
        return;
    
    m_celeris->set_start(pose);
}

void NewCelerisUserController::place_planner_goal(const Camera& camera) {
    LOG_METHOD();

    NonholonomicPos pose;
    if (!make_pose_from_camera(camera, pose))
        return;
    
    m_celeris->set_goal(pose);
}
