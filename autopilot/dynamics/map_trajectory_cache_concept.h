#pragma once

#include <map>

#include "clock.h"
#include "trajectory_cache_concept.h"

namespace celeris {
    /*
        Это просто поразительно, насколько упрощает код
        шаблонный полиморфизм!!!
    */
    template<class Y>
    using MapTrajectoryCache = std::map<simulation::Timestamp, Y>;

    // Небольшая проверка
    static_assert(trajectory_cache<MapTrajectoryCache, int>);
}
