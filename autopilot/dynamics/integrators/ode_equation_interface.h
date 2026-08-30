#pragma once

#include "../../../vulkan_self/logger/logger_header.h"
#include "../clock.h"

namespace celeris {
    template<class State, class Derivative, class Control>
    class OdeEquationInterface {
    public:
        _XPARENT_NAME(OdeEquationInterface)

        virtual ~OdeEquationInterface() noexcept = default;

        [[nodiscard]]
        virtual Derivative calculate_derivative(
            simulation::Timestamp timestamp,
            const State& state,
            const Control& control
        ) const = 0;

        virtual void project_state_inplace(State& state) const {
            // Nothing by default
        }
    };
}
