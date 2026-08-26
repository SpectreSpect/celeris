#pragma once

#include <chrono>

namespace celeris {
    namespace simulation {
        struct Clock {
            using duration = std::chrono::nanoseconds;
            using rep = duration::rep;
            using period = duration::period;
            using time_point = std::chrono::time_point<Clock, duration>;
        };

        using Timestamp = Clock::time_point;
        using Duration = Clock::duration;

        [[nodiscard]]
        constexpr double to_seconds(Duration duration) noexcept {
            return std::chrono::duration<double>{duration}.count();
        }
    }
}
