#pragma once
#include "logger.h"
#include "palette.h"
#include "level_marker.h"
#include "class_name_concept.h"

extern Logger logger;

template <class... Args>
requires ((std::convertible_to<Args, std::string_view>) && ...)
static std::string cat_nms(const Args&... args) {
    std::string result;
    bool first = true;

    auto append = [&](std::string_view s) {
        if (!first) {
            result += "::";
        }
        result += s;
        first = false;
    };

    (append(std::string_view(args)), ...);
    return result;
}

static MultiColorString header_str(std::string_view class_name, std::string_view func_name) {
    return MultiColorString({
        ColoredString(class_name, LoggerPalette::teal),
        ColoredString("::", LoggerPalette::white),
        ColoredString(func_name, LoggerPalette::purple)
    });
}

// Пока пусть здесь лежит...
#define ROBOCROSS_CAT(A, B) A##B
#define XROBOCROSS_CAT(A, B) ROBOCROSS_CAT(A, B)
#define ROBOCROSS_CAT3(A, B, C) A##B##C
#define ROBOCROSS_STR(x) #x
#define ROBOCROSS_XSTR(x) ROBOCROSS_STR(x)

#define LOG_METHOD()                                                             \
    static_assert(                                                               \
        has_class_name<std::remove_cvref_t<decltype(*this)>>,                    \
        "LOG_METHOD() requires static k_class_name in the enclosing class"       \
    );                                                                           \
    LevelMarker XROBOCROSS_CAT(_logger_level_marker_, __LINE__)(                 \
        logger,                                                                  \
        header_str(std::remove_cvref_t<decltype(*this)>::k_class_name, __func__) \
    )

#define LOG_NAMED(NAME)                                          \
    LevelMarker XROBOCROSS_CAT(_logger_level_marker_, __LINE__)( \
        logger,                                                  \
        header_str(NAME, __func__)                               \
    )

#define LOG_FUNC()                                               \
    LevelMarker XROBOCROSS_CAT(_logger_level_marker_, __LINE__)( \
        logger,                                                  \
        ColoredString(__func__, LoggerPalette::purple)           \
    )

using clr = ColoredString; 

//

#define VSCODE_CLR_STREAM(CLASS_NAME, FUNC_NAME) \
clr(CLASS_NAME, LoggerPalette::teal) << "::" << clr(FUNC_NAME, LoggerPalette::purple) << clr("()", LoggerPalette::yellow)