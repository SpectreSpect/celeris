#pragma once

#include <iterator>

#include "events/instant_event.h"
#include "clock.h"

namespace celeris {

    // template<class State, class EventState>
    // class InstantEvent

    template<class State, class... EventStates>
    class EventsContainerInterface {
    public:
        virtual ~EventsContainerInterface() = default;

        virtual std::input_iterator lower_bound(simulation::Timestamp timestamp);
        virtual void upper_bound(simulation::Timestamp timestamp);
    };
}