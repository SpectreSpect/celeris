#pragma once

enum class MouseMode {
    NORMAL,     // visible and free
    HIDDEN,     // hidden but still moves
    DISABLED    // captured/locked for FPS-style control
};