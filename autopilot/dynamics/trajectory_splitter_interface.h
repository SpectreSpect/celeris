#pragma once

#include <vector>

#include "clock.h"

namespace celeris {
    template<class Y>
    class TrajectorySplitterInterface {
    public:
        using Timestamp = simulation::Timestamp;
        using Duration = simulation::Duration;

        // Возвращает точки разреза интервала
        virtual std::vector<Timestamp> split(Timestamp from, Timestamp to, const Y& value) = 0;
    };
}
