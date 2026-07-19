#include "celeris_user_controller.h"

#include "../vulkan_self/keyboard_input_reciever.h"
#include "../imgui_layer.h"
#include "celeris_visualizer.h"
#include "gamepad_controller.h"

CelerisUserController::CelerisUserController(
    Celeris& celeris, 
    CelerisVisualizer& celeris_visualizer)
    :   m_celeris(&celeris),
        m_celeris_visualizer(&celeris_visualizer)
         {

    m_inflated_voxel_color[0] = float((m_celeris->voxel_grid()->params().inflated_voxel_color >> 24u) & 0xFFu) / 255.0f;
    m_inflated_voxel_color[1] = float((m_celeris->voxel_grid()->params().inflated_voxel_color >> 16u) & 0xFFu) / 255.0f,
    m_inflated_voxel_color[2] = float((m_celeris->voxel_grid()->params().inflated_voxel_color >> 8u) & 0xFFu) / 255.0f;
    m_inflated_voxel_color[3] = float(m_celeris->voxel_grid()->params().inflated_voxel_color & 0xFFu) / 255.0f;


    m_inflated_curvature_limit_exceeded_voxel_color[0] = float((m_celeris->voxel_grid()->params().inflated_curvature_limit_exceeded_voxel_color >> 24u) & 0xFFu) / 255.0f;
    m_inflated_curvature_limit_exceeded_voxel_color[1] = float((m_celeris->voxel_grid()->params().inflated_curvature_limit_exceeded_voxel_color >> 16u) & 0xFFu) / 255.0f;
    m_inflated_curvature_limit_exceeded_voxel_color[2] = float((m_celeris->voxel_grid()->params().inflated_curvature_limit_exceeded_voxel_color >> 8u) & 0xFFu) / 255.0f;
    m_inflated_curvature_limit_exceeded_voxel_color[3] = float(m_celeris->voxel_grid()->params().inflated_curvature_limit_exceeded_voxel_color & 0xFFu) / 255.0f;

    m_negative_x_inflation_size = static_cast<int>(m_celeris->voxel_grid()->params().negative_x_inflation_size);
    m_positive_x_inflation_size = static_cast<int>(m_celeris->voxel_grid()->params().positive_x_inflation_size);
    m_negative_y_inflation_size = static_cast<int>(m_celeris->voxel_grid()->params().negative_y_inflation_size);
    m_positive_y_inflation_size = static_cast<int>(m_celeris->voxel_grid()->params().positive_y_inflation_size);
    m_negative_z_inflation_size = static_cast<int>(m_celeris->voxel_grid()->params().negative_z_inflation_size);
    m_positive_z_inflation_size = static_cast<int>(m_celeris->voxel_grid()->params().positive_z_inflation_size);
}

void CelerisUserController::update(
    float delta_time, 
    Camera& camera, 
    KeyboardInputReciever& keyboard_input_reciever,
    FPSCameraController& fps_camera_controller) {
    if (m_car_playback_active) {
        m_car_playback_distance += m_car_playback_speed * delta_time;

        if (m_car_playback_distance >= m_car_playback_total_length) {
            if (m_car_playback_loop) {
                m_car_playback_distance = std::fmod(m_car_playback_distance, m_car_playback_total_length);
            } else {
                m_car_playback_distance = m_car_playback_total_length;
                m_car_playback_active = false;
            }
        }

        m_celeris_visualizer->set_car_pose_override(
            sample_path_pose(m_car_playback_path, m_car_playback_distance)
        );
    }

    if (keyboard_input_reciever.on_key_pressed(GLFW_KEY_F)) {
        use_fps_camera_controller(fps_camera_controller);
    }
    if (keyboard_input_reciever.on_key_pressed(GLFW_KEY_R)) {
        use_third_person_camera_controller();
    }

    if (keyboard_input_reciever.on_key_pressed(GLFW_KEY_1))
        place_start(camera);
    if (keyboard_input_reciever.on_key_pressed(GLFW_KEY_2))
        place_end(camera);
    if (keyboard_input_reciever.on_key_pressed(GLFW_KEY_3))
        start_path_planning();
    if (keyboard_input_reciever.on_key_pressed(GLFW_KEY_4))
        move_start_to_vehicle();
    if (keyboard_input_reciever.on_key_pressed(GLFW_KEY_5))
        m_celeris->visualize_active_path_potential();
    if (keyboard_input_reciever.on_key_pressed(GLFW_KEY_T))
        add_directional_waypoint(camera);
    if (keyboard_input_reciever.on_key_pressed(GLFW_KEY_G))
        add_nondirectional_waypoint(camera);
    if (keyboard_input_reciever.on_key_pressed(GLFW_KEY_L))
        delete_last_waypoint();
    if (keyboard_input_reciever.on_key_pressed(GLFW_KEY_N)) {
    }
}

void CelerisUserController::display_interface(Camera& camera, GamepadController& gamepad_controller) {
    ImGui::Begin("Debug");

    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

    ImGui::TextUnformatted("Camera position:");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "x: %.2f", camera.position.x);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "y: %.2f", camera.position.y);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 0.35f, 1.0f, 1.0f), "z: %.2f", camera.position.z);

    if (ImGui::CollapsingHeader("Voxel grid debug")) {
        ImGui::Checkbox("Show voxel grid", &m_show_voxel_grid);

        bool inflated_settings_changed = false;
        inflated_settings_changed |= ImGui::Checkbox(
            "Display inflated voxels",
            &m_display_inflated_voxels
        );
        inflated_settings_changed |= ImGui::ColorEdit4(
            "Inflated voxel color",
            m_inflated_voxel_color,
            ImGuiColorEditFlags_AlphaBar
        );
        inflated_settings_changed |= ImGui::ColorEdit4(
            "Inflated curvature-limit voxel color",
            m_inflated_curvature_limit_exceeded_voxel_color,
            ImGuiColorEditFlags_AlphaBar
        );

        inflated_settings_changed |= ImGui::SliderInt(
            "Negative X inflation size",
            &m_negative_x_inflation_size,
            0,
            static_cast<int>(m_celeris->voxel_grid()->params().chunk_size.x)
        );
        inflated_settings_changed |= ImGui::SliderInt(
            "Positive X inflation size",
            &m_positive_x_inflation_size,
            0,
            static_cast<int>(m_celeris->voxel_grid()->params().chunk_size.x)
        );
        inflated_settings_changed |= ImGui::SliderInt(
            "Negative Y inflation size",
            &m_negative_y_inflation_size,
            0,
            static_cast<int>(m_celeris->voxel_grid()->params().chunk_size.y)
        );
        inflated_settings_changed |= ImGui::SliderInt(
            "Positive Y inflation size",
            &m_positive_y_inflation_size,
            0,
            static_cast<int>(m_celeris->voxel_grid()->params().chunk_size.y)
        );
        inflated_settings_changed |= ImGui::SliderInt(
            "Negative Z inflation size",
            &m_negative_z_inflation_size,
            0,
            static_cast<int>(m_celeris->voxel_grid()->params().chunk_size.z)
        );
        inflated_settings_changed |= ImGui::SliderInt(
            "Positive Z inflation size",
            &m_positive_z_inflation_size,
            0,
            static_cast<int>(m_celeris->voxel_grid()->params().chunk_size.z)
        );

        if (inflated_settings_changed) {
            m_celeris->voxel_grid()->set_inflated_voxel_debug_display(
                m_display_inflated_voxels ? 1u : 0u,
                pack_inflated_voxel_color(m_inflated_voxel_color),
                pack_inflated_voxel_color(m_inflated_curvature_limit_exceeded_voxel_color),
                static_cast<uint32_t>(m_negative_x_inflation_size),
                static_cast<uint32_t>(m_positive_x_inflation_size),
                static_cast<uint32_t>(m_negative_y_inflation_size),
                static_cast<uint32_t>(m_positive_y_inflation_size),
                static_cast<uint32_t>(m_negative_z_inflation_size),
                static_cast<uint32_t>(m_positive_z_inflation_size)
            );
        }
    }

    if (ImGui::CollapsingHeader("Voxel point map")) {
        ImGui::InputText("Save file", m_map_save_file_name, sizeof(m_map_save_file_name));

        if (ImGui::Button("Save map")) {
            std::filesystem::path save_file_name =
                std::filesystem::path(m_map_save_file_name).filename();

            if (save_file_name.empty()) {
                m_map_save_failed = true;
                m_map_save_status = "File name is empty";
            } else {
                save_file_name.replace_extension(".vpm");
                const std::filesystem::path save_path = m_saved_maps_directory / save_file_name;

                try {
                    m_celeris->save_map(save_path);
                    m_map_save_failed = false;
                    m_map_save_status = "Saved " + save_path.string();
                } catch (const std::exception& error) {
                    m_map_save_failed = true;
                    m_map_save_status = error.what();
                }
            }
        }

        if (!m_map_save_status.empty()) {
            const ImVec4 color = m_map_save_failed
                ? ImVec4(1.0f, 0.25f, 0.25f, 1.0f)
                : ImVec4(0.35f, 1.0f, 0.45f, 1.0f);
            ImGui::TextColored(color, "%s", m_map_save_status.c_str());
        }

        if (ImGui::Button("Localize on map")) {
            try {
                const bool localized = m_celeris->localize_on_map();
                m_map_localization_failed = !localized;
                m_map_localization_status = localized
                    ? "Localized on map"
                    : "Localization failed";
            } catch (const std::exception& error) {
                m_map_localization_failed = true;
                m_map_localization_status = error.what();
            }
        }

        if (!m_map_localization_status.empty()) {
            const ImVec4 color = m_map_localization_failed
                ? ImVec4(1.0f, 0.25f, 0.25f, 1.0f)
                : ImVec4(0.35f, 1.0f, 0.45f, 1.0f);
            ImGui::TextColored(color, "%s", m_map_localization_status.c_str());
        }
    }

    m_celeris_visualizer->display_debug_controls();
    m_celeris->display_path_planner_debug_controls();

    if (ImGui::CollapsingHeader("Path planning controls")) {
        if (ImGui::Button("Place start")) {
            place_start(camera);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Key: 1");

        if (ImGui::Button("Place end")) {
            place_end(camera);
        }

        ImGui::SameLine();
        ImGui::TextUnformatted("Key: 2");

        if (ImGui::Button("Start path planning")) {
            start_path_planning();
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Key: 3");

        if (ImGui::Button("Move start to vehicle")) {
            move_start_to_vehicle();
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Key: 4");

        const std::vector<Celeris::Waypoint>& waypoints = m_celeris->waypoints();
        const size_t directional_waypoint_count = std::count_if(
            waypoints.begin(),
            waypoints.end(),
            [](const Celeris::Waypoint& waypoint) {
                return waypoint.directional();
            }
        );

        ImGui::Separator();
        ImGui::Text(
            "Waypoints: %zu (%zu directional)",
            waypoints.size(),
            directional_waypoint_count
        );
        ImGui::Text(
            "Active waypoint: %zu%s",
            m_celeris->active_waypoint_index(),
            m_celeris->waypoint_path_completed() ? " (complete)" : ""
        );

        float waypoint_reach_radius = m_celeris->waypoint_reach_radius();
        if (ImGui::SliderFloat(
                "Waypoint reach radius",
                &waypoint_reach_radius,
                0.1f,
                20.0f,
                "%.2f m")) {
            m_celeris->set_waypoint_reach_radius(waypoint_reach_radius);
        }

        ImGui::InputText(
            "Waypoint path file",
            m_waypoint_path_file_name,
            sizeof(m_waypoint_path_file_name)
        );

        if (ImGui::Button("Save waypoint path")) {
            std::filesystem::path save_file_name =
                std::filesystem::path(m_waypoint_path_file_name).filename();

            if (save_file_name.empty()) {
                m_waypoint_path_failed = true;
                m_waypoint_path_status = "File name is empty";
            } else {
                save_file_name.replace_extension(".wpp");
                const std::filesystem::path save_path =
                    m_saved_waypoint_paths_directory / save_file_name;

                try {
                    m_celeris->save_waypoint_path(save_path);
                    m_waypoint_path_failed = false;
                    m_waypoint_path_status = "Saved " + save_path.string();
                } catch (const std::exception& error) {
                    m_waypoint_path_failed = true;
                    m_waypoint_path_status = error.what();
                }
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Load waypoint path")) {
            std::filesystem::path load_file_name =
                std::filesystem::path(m_waypoint_path_file_name).filename();

            if (load_file_name.empty()) {
                m_waypoint_path_failed = true;
                m_waypoint_path_status = "File name is empty";
            } else {
                load_file_name.replace_extension(".wpp");
                const std::filesystem::path load_path =
                m_saved_waypoint_paths_directory / load_file_name;

                try {
                    m_celeris->load_waypoint_path(load_path);
                    m_waypoint_path_failed = false;
                    m_waypoint_path_status = "Loaded " + load_path.string();
                } catch (const std::exception& error) {
                    m_waypoint_path_failed = true;
                    m_waypoint_path_status = error.what();
                }
            }
        }

        if (!m_waypoint_path_status.empty()) {
            const ImVec4 color = m_waypoint_path_failed
                ? ImVec4(1.0f, 0.25f, 0.25f, 1.0f)
                : ImVec4(0.35f, 1.0f, 0.45f, 1.0f);
            ImGui::TextColored(color, "%s", m_waypoint_path_status.c_str());
        }

        if (ImGui::Button("Add directional waypoint")) {
            add_directional_waypoint(camera);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Key: T");

        if (ImGui::Button("Add nondirectional waypoint")) {
            add_nondirectional_waypoint(camera);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Key: G");

        if (ImGui::Button("Delete last waypoint")) {
            delete_last_waypoint();
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("Key: L");

        ImGui::Separator();

        bool gamepad_commands_enabled = m_celeris->gamepad_commands_enabled();
        if (ImGui::Checkbox("Gamepad driving", &gamepad_commands_enabled)) {
            m_celeris->set_gamepad_commands_enabled(gamepad_commands_enabled);
        }

        bool print_gamepad_input = gamepad_controller.print_input_events();
        if (ImGui::Checkbox("Print gamepad input", &print_gamepad_input)) {
            gamepad_controller.set_print_input_events(print_gamepad_input);
        }

        const VehicleCommand gamepad_command = m_celeris->gamepad_command();
        ImGui::Text(
            "Gamepad: %s%s",
            gamepad_controller.gamepad_present() ? "gamepad" :
            (gamepad_controller.raw_joystick_present() ? "raw joystick" : "not connected"),
            gamepad_controller.brake_pressed() ? " (R1 brake)" : ""
        );
        ImGui::Text(
            "Gamepad command: acceleration %.2f, steering velocity %.2f",
            gamepad_command.acceleration,
            gamepad_command.steering_angle_velocity
        );
        ImGui::Text(
            "Command sender: %s, %s",
            m_celeris->command_sender_running() ? "running" : "stopped",
            m_celeris->command_sender_connected() ? "connected" : "not connected"
        );

        ImGui::Separator();

        if (ImGui::Button("Play car path")) {
            start_car_playback();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            m_car_playback_active = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset car")) {
            m_car_playback_active = false;
            m_celeris_visualizer->clear_car_pose_override();
        }

        ImGui::Checkbox("Loop car path", &m_car_playback_loop);
        ImGui::SliderFloat("Car playback speed", &m_car_playback_speed, 0.1f, 20.0f, "%.1f m/s");

    }

    ImGui::End();
}