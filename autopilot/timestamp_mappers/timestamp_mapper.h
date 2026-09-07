#pragma once

#include <optional>
#include <cstdint>
#include <chrono>

#include "../../vulkan_self/logger/logger_header.h"
#include "../dynamics/clock.h"

namespace celeris {
    class TimestampMapper {
    public:
        _XPARENT_NAME(TimestampMapper);

        using time_point = std::chrono::steady_clock::time_point;
        
        virtual ~TimestampMapper() = default;

        /*
            Отображает `timestamp_ns` в некоторую временную шкалу.

            1. timestamp_ns - время в исходной временной шкале (в наносекундах).
            2. received_at - момент времени, когда было получено измерение времени `timestamp_ns`. Измеряется
            в шкале системных steady_clock.
        */

        [[nodiscard]]
        virtual std::optional<simulation::Timestamp> map(
            std::int64_t timestamp_ns,
            time_point received_at
        ) = 0;
    };
}
