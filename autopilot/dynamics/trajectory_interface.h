#pragma once

#include "../../vulkan_self/logger/logger_header.h"
#include "clock.h"

namespace celeris {
    template<class Y>
    class TrajectoryInterface {
    public:
        _XPARENT_NAME(TrajectoryInterface);

        using Timestamp = simulation::Timestamp;

        virtual Y define_trajectory(Timestamp timestamp) = 0;
    };
}
