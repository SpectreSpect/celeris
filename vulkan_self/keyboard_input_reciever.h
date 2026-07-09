#pragma once

#include <unordered_map>

#include "logger/logger_header.h"
#include "window.h"

class KeyboardInputReciever {
public:
    _XCLASS_NAME(KeyboardInputReciever);

    KeyboardInputReciever(Window& window);

    bool on_key_pressed(int glfw_key_code);
    void update();

private:
    Window* m_window = nullptr;
    std::unordered_map<int, bool> m_key_press_states;
    std::unordered_map<int, bool> m_on_key_press_states;
};
