#pragma once 

#include "../../../vulkan_self/logger/logger_header.h"
#include "../clock.h"
#include "ode_equation_interface.h"

namespace celeris {
    template<class State, class Derivative, class Control>
    class OdeIntegratorInterface {
    public:
        _XPARENT_NAME(OdeIntegratorInterface)

        virtual ~OdeIntegratorInterface() noexcept = default;

        virtual void integrate_inplace(
            simulation::Timestamp time_start,
            simulation::Timestamp time_end,
            State& initial_state,
            const Control& control,
            const OdeEquationInterface<State, Derivative, Control>& equation,
            simulation::Duration delta_time
        ) const = 0;

        [[nodiscard]]
        State integrate(
            simulation::Timestamp time_start,
            simulation::Timestamp time_end,
            State initial_state,
            const Control& control,
            const OdeEquationInterface<State, Derivative, Control>& equation,
            simulation::Duration delta_time) const
        {
            LOG_METHOD();

            integrate_inplace(time_start, time_end, initial_state, control, equation, delta_time);
            return initial_state;
        }
    };
}
