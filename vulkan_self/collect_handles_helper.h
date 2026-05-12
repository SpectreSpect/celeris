#pragma once
#include <concepts>
#include <type_traits>
#include <utility>
#include <span>

#include "logger/logger_header.h"

template<class T>
concept HasHandle = requires(const T& object) {
    { object.handle() };
};

template<class T>
using HandleOf = std::remove_cvref_t<decltype(std::declval<const T&>().handle())>;

template<class Range>
using RangeElement = std::remove_cvref_t<std::ranges::range_reference_t<Range>>;

template<class Range>
requires std::ranges::input_range<Range> && HasHandle<RangeElement<Range>>
std::vector<HandleOf<RangeElement<Range>>> collect_handles(Range&& objects) {
    LOG_FUNC();

    using T = RangeElement<Range>;

    std::vector<HandleOf<T>> handles;
    if constexpr (std::ranges::sized_range<Range>) {
        handles.reserve(std::ranges::size(objects));
    }

    for (const auto& object : objects) {
        handles.push_back(object.handle());
    }

    return handles;
}
