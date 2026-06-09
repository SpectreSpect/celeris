#pragma once
#include <string>
#include <vector>
#include <utility>
#include <cstdint>
#include <ostream>
#include <concepts>
#include <optional>
#include <type_traits>
#include <string_view>
#include <initializer_list>

#include "colored_string.h"

struct MultiColorString {
    std::vector<ColoredString> strings;
    
    MultiColorString() = default;
    MultiColorString(std::vector<ColoredString> strings);
    MultiColorString(std::initializer_list<ColoredString> args);
    MultiColorString(ColoredString str);

    template <class Str>
    requires std::constructible_from<ColoredString, Str&&> &&
             (!std::same_as<std::remove_cvref_t<Str>, ColoredString>) &&
             (!std::same_as<std::remove_cvref_t<Str>, MultiColorString>)
    explicit MultiColorString(Str&& str) { 
        strings.emplace_back(std::forward<Str>(str));
    }

    std::string to_string(std::optional<uint32_t> default_color_rgb = std::nullopt) const;
    ColoredString to_monocolor_string(std::optional<uint32_t> color_rgb = std::nullopt) const;
    operator std::string() const;
    MultiColorString& operator+=(const MultiColorString& off); 
    friend std::ostream& operator<<(std::ostream& os, const MultiColorString& value);
};
