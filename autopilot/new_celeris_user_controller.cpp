#include "new_celeris_user_controller.h"

#include "../voxel_grid_vulkan/voxel_grid.h"
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

void NewCelerisUserController::update() {
    m_celeris_visualizer->gazelle_next_visible(m_config.show_gazelle_next);
    m_celeris_visualizer->voxel_grid_visible(m_config.show_voxel_grid);
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

void NewCelerisUserController::display_interface(Camera& camera) {
    display_celeris_general_panel(camera);
    display_voxel_grid_panel();
}