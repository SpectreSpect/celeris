#pragma once

#include "vehicle_command_sender.h"

#include <GLFW/glfw3.h>

#include <array>
#include <vector>

class GamepadController {
public:
    struct Desc {
        int joystick_id = GLFW_JOYSTICK_1;
        float axis_epsilon = 0.05f;
        float axis_deadzone = 0.12f;
        float max_steering_angle = 0.4f;
        int raw_left_stick_x_axis = 0;
        int raw_left_stick_y_axis = 1;
        int raw_right_bumper_button = 5;
        bool print_input_events = true;
    };

    GamepadController();
    explicit GamepadController(Desc desc);

    void update(float max_speed);

    VehicleCommand command() const noexcept;
    bool gamepad_present() const noexcept;
    bool raw_joystick_present() const noexcept;
    bool brake_pressed() const noexcept;

    void set_print_input_events(bool enabled) noexcept;
    bool print_input_events() const noexcept;

private:
    Desc m_desc;
    VehicleCommand m_command;
    bool m_gamepad_present = false;
    bool m_raw_joystick_present = false;
    bool m_brake_pressed = false;

    std::array<float, GLFW_GAMEPAD_AXIS_LAST + 1> m_previous_gamepad_axes{};
    std::array<unsigned char, GLFW_GAMEPAD_BUTTON_LAST + 1> m_previous_gamepad_buttons{};
    std::vector<float> m_previous_raw_joystick_axes;
    std::vector<unsigned char> m_previous_raw_joystick_buttons;

    float apply_deadzone(float value) const noexcept;
    void update_gamepad_command(const GLFWgamepadstate& state, float max_speed);
    void update_raw_joystick_command(float max_speed);
    void print_gamepad_events(const GLFWgamepadstate& state);
    void print_raw_joystick_events();
    void reset_gamepad_history();
    void reset_raw_joystick_history();
};
