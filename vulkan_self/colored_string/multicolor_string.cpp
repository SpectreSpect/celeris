#include "multicolor_string.h"
#include "colored_string.h"

MultiColorString::MultiColorString(std::vector<ColoredString> strings) : strings(std::move(strings)) {}
MultiColorString::MultiColorString(std::initializer_list<ColoredString> args) : strings(args) {}
MultiColorString::MultiColorString(ColoredString str) : strings({std::move(str)}) {}

std::string MultiColorString::to_string(std::optional<uint32_t> default_color_rgb) const {
    std::string result;

    for (const ColoredString& s : strings) {
        if (s.color_rgb.has_value() || !default_color_rgb.has_value())
            result += s;
        else
            result += ColoredString::to_color_string(s.str, *default_color_rgb);
    }

    return result;
}

ColoredString MultiColorString::to_monocolor_string(std::optional<uint32_t> color_rgb) const {
    ColoredString result;
    result.color_rgb = color_rgb;

    for (const ColoredString& s : strings) {
        result.str += s.str;
    }

    return result;
}

MultiColorString::operator std::string() const {
    return to_string();
}

MultiColorString& MultiColorString::operator+=(const MultiColorString& off) {
    strings.insert(strings.end(), off.strings.begin(), off.strings.end());
    return *this;
}

std::ostream& operator<<(std::ostream& os, const MultiColorString& value) {
    return os << static_cast<std::string>(value);
}
