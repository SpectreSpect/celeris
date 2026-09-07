#pragma once

#include <concepts>
#include <utility>

#include "../clock.h"
#include "instant_event.h"

namespace celeris {
    template<class State, class Replacement>
    concept selectively_replaceable_from =
        requires(State& destination, const Replacement& replacement)
    {
        { destination ^= replacement } -> std::same_as<State&>;
    };

    template<class State, class Replacement>
    class SelectiveReplacementEvent {
        static_assert(
            selectively_replaceable_from<State, Replacement>,
            "State cannot be selectively replaced from Replacement. "
            "You need to implement the 'State ^= Replacement' operator."
        );
    };

    template<class State, class Replacement>
    requires (selectively_replaceable_from<State, Replacement>)
    class SelectiveReplacementEvent<State, Replacement> : public InstantEvent<State> {
    public:
        _XCHILD_NAME(SelectiveReplacementEvent);

        SelectiveReplacementEvent(simulation::Timestamp timestamp, Replacement replacement)
        :   InstantEvent<State>(timestamp),
            m_replacement(std::move(replacement)) {}

        void apply(StateEstimate<State>& state) const override {
            LOG_METHOD();

            state.state ^= m_replacement;
        }

    private:
        Replacement m_replacement;
    };
}
