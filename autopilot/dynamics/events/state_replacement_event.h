#pragma once

#include <utility>

#include "instant_event.h"
#include "../../../vulkan_self/logger/logger_header.h"

namespace celeris {
    template<class State>
    class StateReplacementEvent : public InstantEvent<State> {
    public:
        _XCHILD_NAME(StateReplacementEvent);

        StateReplacementEvent(simulation::Timestamp timestamp, State state)
            :   InstantEvent<State>(timestamp), 
                m_state(std::move(state)) {}

        void apply(StateEstimate<State>& state) const override {
            LOG_METHOD();

            state.state = m_state;
        }
    
    private:
        State m_state;
    };
}
