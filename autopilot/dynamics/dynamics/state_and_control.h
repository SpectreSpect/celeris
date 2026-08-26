#pragma once

namespace celeris {
    template<class State, class Control>
    struct StateAndControl {
        State state;
        Control control;
    };
}
