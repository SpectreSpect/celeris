#pragma once
#include <vector>
#include <functional>
#include <concepts>

#include "multicolor_string.h"

class Logger;

struct ColoredStringsStream {
    /*
        В теории можно было бы записать strings в виде одной MultiColorString, но 
        тогда строки не будут логически разделены друг между другом, что может быть не удобно.
    */
    std::vector<MultiColorString> strings;
    std::function<void(const MultiColorString&)> destroy_callback;

    ColoredStringsStream() = default;
    ColoredStringsStream(std::function<void(const MultiColorString&)> destroy_callback);

    ~ColoredStringsStream() noexcept;

    template<class _ClrStr>
    requires std::constructible_from<MultiColorString, _ClrStr&&>
    inline ColoredStringsStream& operator<<(_ClrStr&& str) noexcept {
        strings.emplace_back(std::forward<_ClrStr>(str));
        return *this;
    }
};
