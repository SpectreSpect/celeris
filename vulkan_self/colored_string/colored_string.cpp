#include "colored_string.h"

ColoredString::ColoredString(std::string_view str) : str(str) { }
ColoredString::ColoredString(std::string_view str, uint32_t color_rgb) : str(str), color_rgb(color_rgb) { }
ColoredString::ColoredString(std::string_view str, std::array<uint8_t, 3> color_rgb) 
    :   str(str), color_rgb((color_rgb[0] << 16) | (color_rgb[1] << 8) | color_rgb[2]) { }
ColoredString::ColoredString(std::string_view str, std::string_view hex_rgb) 
    : str(str), color_rgb(parse_color(hex_rgb)) { }

ColoredString::operator std::string() const {
    if (color_rgb.has_value())
        return to_color_string(str, *color_rgb);
    else
        return str;
}

std::ostream& operator<<(std::ostream& os, const ColoredString& value) {
    return os << static_cast<std::string>(value);
}

std::string ColoredString::to_color_string(std::string_view message, std::array<uint8_t, 3> color_rgb) {
    std::string r = std::to_string(color_rgb[0]);
    std::string g = std::to_string(color_rgb[1]);
    std::string b = std::to_string(color_rgb[2]);

    std::string colored_message = "\033[38;2;" + r + ";" + g + ";" + b + "m" + std::string(message) + "\033[0m";
    return colored_message;
}

std::string ColoredString::to_color_string(std::string_view message, uint32_t color_rgb) {
    uint8_t r = (color_rgb >> 16) & 0xFFu;
    uint8_t g = (color_rgb >> 8) & 0xFFu;
    uint8_t b = color_rgb & 0xFFu;
    
    return to_color_string(message, {r, g, b});
}

std::uint32_t ColoredString::parse_color(std::string_view hex_rgb) {
    if (hex_rgb.size() != 7 || hex_rgb[0] != '#') {
        std::string message = "Expected string like #rrggbb (r, g, b - without a!)";
        std::cout << message << std::endl;
        throw std::invalid_argument(message);
    }

    return static_cast<std::uint32_t>(std::stoul(std::string(hex_rgb).substr(1), nullptr, 16));
}
