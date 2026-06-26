#pragma once
#include "mouse_mode.h"

struct MouseState {
    bool initialized = false;
    double x = 0; 
    double y = 0;
    bool left_pressed = false;
    bool right_pressed = false;
    bool middle_pressed = false;
    MouseMode mode = MouseMode::NORMAL;
};