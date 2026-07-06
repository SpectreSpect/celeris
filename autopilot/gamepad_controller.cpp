#include "gamepad_controller.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {
    constexpr std::array<const char*, GLFW_GAMEPAD_AXIS_LAST + 1> gamepad_axis_names{
        "left stick x",
        "left stick y",
        "right stick x",
        "right stick y",
        "left trigger",
        "right trigger"
    };

    constexpr std::array<const char*, GLFW_GAMEPAD_BUTTON_LAST + 1> gamepad_button_names{
        "A / cross",
        "B / circle",
        "X / square",
        "Y / triangle",
        "left bumper",
        "right bumper",
        "back / share",
        "start / options",
        "guide / PS",
        "left stick",
        "right stick",
        "d-pad up",
        "d-pad right",
        "d-pad down",
        "d-pad left"
    };
}

GamepadController::GamepadController()
    : GamepadController(Desc{})
{
}

GamepadController::GamepadController(Desc desc)
    : m_desc(desc)
{
    reset_gamepad_history();
}

void GamepadController::update(float max_speed) {
    GLFWgamepadstate gamepad_state{};
    const bool gamepad_present = glfwJoystickIsGamepad(m_desc.joystick_id) &&
                                 glfwGetGamepadState(m_desc.joystick_id, &gamepad_state);

    if (gamepad_present != m_gamepad_present) {
        if (m_desc.print_input_events) {
            if (gamepad_present) {
                const char* name = glfwGetGamepadName(m_desc.joystick_id);
                std::cout << "Gamepad connected: "
                          << (name ? name : "unknown")
                          << std::endl;
            } else if (m_gamepad_present) {
                std::cout << "Gamepad disconnected" << std::endl;
            }
        }

        reset_gamepad_history();
        m_gamepad_present = gamepad_present;
    }

    if (gamepad_present) {
        update_gamepad_command(gamepad_state, max_speed);
        if (m_desc.print_input_events)
            print_gamepad_events(gamepad_state);

        m_raw_joystick_present = false;
        reset_raw_joystick_history();
        return;
    }

    m_command = VehicleCommand{};
    m_brake_pressed = false;

    const bool raw_present = glfwJoystickPresent(m_desc.joystick_id);
    if (raw_present != m_raw_joystick_present) {
        if (m_desc.print_input_events) {
            if (raw_present) {
                const char* name = glfwGetJoystickName(m_desc.joystick_id);
                std::cout << "Raw joystick connected: "
                          << (name ? name : "unknown")
                          << std::endl;
            } else if (m_raw_joystick_present) {
                std::cout << "Raw joystick disconnected" << std::endl;
            }
        }

        reset_raw_joystick_history();
        m_raw_joystick_present = raw_present;
    }

    if (raw_present) {
        update_raw_joystick_command(max_speed);
    }

    if (raw_present && m_desc.print_input_events)
        print_raw_joystick_events();
}

VehicleCommand GamepadController::command() const noexcept {
    return m_command;
}

bool GamepadController::gamepad_present() const noexcept {
    return m_gamepad_present;
}

bool GamepadController::raw_joystick_present() const noexcept {
    return m_raw_joystick_present;
}

bool GamepadController::brake_pressed() const noexcept {
    return m_brake_pressed;
}

void GamepadController::set_print_input_events(bool enabled) noexcept {
    m_desc.print_input_events = enabled;
}

bool GamepadController::print_input_events() const noexcept {
    return m_desc.print_input_events;
}

float GamepadController::apply_deadzone(float value) const noexcept {
    const float deadzone = std::clamp(m_desc.axis_deadzone, 0.0f, 0.99f);
    const float magnitude = std::abs(value);
    if (magnitude <= deadzone)
        return 0.0f;

    const float normalized = (magnitude - deadzone) / (1.0f - deadzone);
    return std::copysign(std::clamp(normalized, 0.0f, 1.0f), value);
}

void GamepadController::update_gamepad_command(
    const GLFWgamepadstate& state,
    float max_speed)
{
    m_brake_pressed = state.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS;
    if (m_brake_pressed) {
        m_command = VehicleCommand{};
        return;
    }

    const float steering_axis = apply_deadzone(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
    const float speed_axis = apply_deadzone(state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
    const float finite_max_speed = std::isfinite(max_speed) ? max_speed : 0.0f;

    m_command = VehicleCommand{
        .speed = -speed_axis * finite_max_speed,
        .steering_angle = steering_axis * m_desc.max_steering_angle
    };
}

void GamepadController::update_raw_joystick_command(float max_speed) {
    int axis_count = 0;
    const float* axes = glfwGetJoystickAxes(m_desc.joystick_id, &axis_count);
    int button_count = 0;
    const unsigned char* buttons = glfwGetJoystickButtons(m_desc.joystick_id, &button_count);

    m_brake_pressed =
        buttons &&
        m_desc.raw_right_bumper_button >= 0 &&
        m_desc.raw_right_bumper_button < button_count &&
        buttons[m_desc.raw_right_bumper_button] == GLFW_PRESS;

    if (m_brake_pressed) {
        m_command = VehicleCommand{};
        return;
    }

    if (!axes ||
        m_desc.raw_left_stick_x_axis < 0 ||
        m_desc.raw_left_stick_y_axis < 0 ||
        m_desc.raw_left_stick_x_axis >= axis_count ||
        m_desc.raw_left_stick_y_axis >= axis_count) {
        m_command = VehicleCommand{};
        return;
    }

    const float steering_axis = apply_deadzone(
        std::clamp(axes[m_desc.raw_left_stick_x_axis], -1.0f, 1.0f)
    );
    const float speed_axis = apply_deadzone(
        std::clamp(axes[m_desc.raw_left_stick_y_axis], -1.0f, 1.0f)
    );
    const float finite_max_speed = std::isfinite(max_speed) ? max_speed : 0.0f;

    m_command = VehicleCommand{
        .speed = -speed_axis * finite_max_speed,
        .steering_angle = steering_axis * m_desc.max_steering_angle
    };
}

void GamepadController::print_gamepad_events(const GLFWgamepadstate& state) {
    for (int i = 0; i <= GLFW_GAMEPAD_BUTTON_LAST; i++) {
        const unsigned char current = state.buttons[i];
        if (current == m_previous_gamepad_buttons[i])
            continue;

        std::cout << "Button " << gamepad_button_names[i] << ' '
                  << (current == GLFW_PRESS ? "pressed" : "released")
                  << std::endl;
        m_previous_gamepad_buttons[i] = current;
    }

    for (int i = 0; i <= GLFW_GAMEPAD_AXIS_LAST; i++) {
        const float current = state.axes[i];
        const float previous = m_previous_gamepad_axes[i];
        const bool moved = std::abs(current - previous) >= m_desc.axis_epsilon;
        const bool active = std::abs(current) >= m_desc.axis_deadzone ||
                            std::abs(previous) >= m_desc.axis_deadzone;

        if (!moved || !active)
            continue;

        std::cout << "Axis " << gamepad_axis_names[i]
                  << ": " << current
                  << std::endl;
        m_previous_gamepad_axes[i] = current;
    }
}

void GamepadController::print_raw_joystick_events() {
    int axis_count = 0;
    const float* axes = glfwGetJoystickAxes(m_desc.joystick_id, &axis_count);
    if (axes) {
        if (m_previous_raw_joystick_axes.size() != static_cast<size_t>(axis_count))
            m_previous_raw_joystick_axes.assign(axis_count, 0.0f);

        for (int i = 0; i < axis_count; i++) {
            const float current = axes[i];
            const float previous = m_previous_raw_joystick_axes[i];
            const bool moved = std::abs(current - previous) >= m_desc.axis_epsilon;
            const bool active = std::abs(current) >= m_desc.axis_deadzone ||
                                std::abs(previous) >= m_desc.axis_deadzone;

            if (!moved || !active)
                continue;

            std::cout << "Raw axis " << i << ": " << current << std::endl;
            m_previous_raw_joystick_axes[i] = current;
        }
    }

    int button_count = 0;
    const unsigned char* buttons = glfwGetJoystickButtons(m_desc.joystick_id, &button_count);
    if (!buttons)
        return;

    if (m_previous_raw_joystick_buttons.size() != static_cast<size_t>(button_count))
        m_previous_raw_joystick_buttons.assign(button_count, GLFW_RELEASE);

    for (int i = 0; i < button_count; i++) {
        const unsigned char current = buttons[i];
        if (current == m_previous_raw_joystick_buttons[i])
            continue;

        std::cout << "Raw button " << i << ' '
                  << (current == GLFW_PRESS ? "pressed" : "released")
                  << std::endl;
        m_previous_raw_joystick_buttons[i] = current;
    }
}

void GamepadController::reset_gamepad_history() {
    m_previous_gamepad_axes.fill(0.0f);
    m_previous_gamepad_buttons.fill(GLFW_RELEASE);
}

void GamepadController::reset_raw_joystick_history() {
    m_previous_raw_joystick_axes.clear();
    m_previous_raw_joystick_buttons.clear();
}
