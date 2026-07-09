#include "keyboard_input_reciever.h"

KeyboardInputReciever::KeyboardInputReciever(Window& window) 
    :   m_window(&window) {}

bool KeyboardInputReciever::on_key_pressed(int glfw_key_code) {
    LOG_METHOD();

    logger().check(m_window, "Window was null");

    auto it = m_key_press_states.find(glfw_key_code);
    if (it == m_key_press_states.end())
        m_key_press_states[glfw_key_code] = false;

    int glfw_key_state = glfwGetKey(m_window->handle(), glfw_key_code);
    
    if (!m_key_press_states[glfw_key_code] && glfw_key_state == GLFW_PRESS) {
        m_key_press_states[glfw_key_code] = true;
        m_on_key_press_states[glfw_key_code] = true;
    }

    if (m_on_key_press_states.find(glfw_key_code) == m_on_key_press_states.end()) {
        m_on_key_press_states[glfw_key_code] = false;
    }

    return m_on_key_press_states[glfw_key_code];
}

void KeyboardInputReciever::update() {
    LOG_METHOD();
    
    for (auto it = m_on_key_press_states.begin(); it != m_on_key_press_states.end(); it++)
        it->second = false;

    for (auto it = m_key_press_states.begin(); it != m_key_press_states.end(); it++) {
        int glfw_key_state = glfwGetKey(m_window->handle(), it->first);
        if (it->second && glfw_key_state == GLFW_RELEASE)
            m_key_press_states[it->first] = false;
    }
}