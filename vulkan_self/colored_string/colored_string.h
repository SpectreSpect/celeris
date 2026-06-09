#pragma once
#include <array>
#include <string>
#include <vector>
#include <cstdint>
#include <ostream>
#include <optional>
#include <iostream>
#include <string_view>

struct ColoredString {
    std::string str = "";
    std::optional<uint32_t> color_rgb = std::nullopt;

    ColoredString() = default;
    ColoredString(std::string_view str);
    ColoredString(std::string_view str, uint32_t color_rgb);
    ColoredString(std::string_view str, std::array<uint8_t, 3> color_rgb);
    ColoredString(std::string_view str, std::string_view hex_rgb);
    
    operator std::string() const;
    friend std::ostream& operator<<(std::ostream& os, const ColoredString& value);

    static std::string to_color_string(std::string_view message, std::array<uint8_t, 3> color_rgb);
    static std::string to_color_string(std::string_view message, uint32_t color_rgb);
    static std::uint32_t parse_color(std::string_view hex_rgb);
};
