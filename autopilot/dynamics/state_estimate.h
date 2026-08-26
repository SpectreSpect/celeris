#pragma once

#include "clock.h"

namespace celeris {
    namespace simulation {
        template<class State>
        struct StateEstimate {
            State state;
            simulation::Timestamp timestamp;
            // В теории сюда можно добавить информацию о "достоверности" состояния...
        };
    }
}
