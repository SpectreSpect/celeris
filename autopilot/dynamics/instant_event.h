#pragma once

#include "clock.h"
#include "state_estimate.h"

namespace celeris {
    namespace simulation {
        template<class State>
        class InstantEvent {
        public:
            _XPARENT_NAME(InstantEvent);

            virtual ~InstantEvent() noexcept = default;

            virtual simulation::Timestamp timestamp() const noexcept = 0;
            virtual void apply(StateEstimate<State>& state) const = 0;
        };
    }
}
