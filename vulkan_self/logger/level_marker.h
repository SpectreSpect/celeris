#pragma once
#include "../colored_string/colored_string.h"
#include "../colored_string/multicolor_string.h"

class Logger;

class LevelMarker {
public:
    LevelMarker() = delete; // Не должно быть дефолтного конструктора
    LevelMarker(Logger& logger, const MultiColorString& level_name);
    ~LevelMarker() noexcept;

    // На всякий случай
    LevelMarker(const LevelMarker&) = delete;
    LevelMarker& operator=(const LevelMarker&) = delete;
    LevelMarker(LevelMarker&&) noexcept = delete;
    LevelMarker& operator=(LevelMarker&&) noexcept = delete;

private:
    Logger* logger = nullptr;
};
