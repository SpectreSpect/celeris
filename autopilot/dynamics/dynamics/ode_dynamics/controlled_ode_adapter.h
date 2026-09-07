#pragma once

#include <type_traits>
#include <concepts>
#include <utility>

#include "../../../../vulkan_self/logger/logger_header.h"
#include "../../integrators/ode_integrator_interface.h"
#include "../../integrators/ode_equation_interface.h"
#include "../dynamic_interface.h"
#include "../../clock.h"
#include "../state_and_control.h"

namespace celeris {
    template<class State, class Derivative, class Control, class Equation>
    concept equation_derived = std::derived_from<Equation, OdeEquationInterface<State, Derivative, Control>>;

    template<class State, class Derivative, class Control, class Integrator>
    concept integrator_derived = std::derived_from<Integrator, OdeIntegratorInterface<State, Derivative, Control>>;

    template<class State, class Derivative, class Control, class Equation, class Integrator>
    class ControlledOdeAdapter {
        static_assert(
            equation_derived<State, Derivative, Control, Equation>,
            "The `Equation` type must derive from"
            "the `OdeEquationInterface<State, Derivative, Control>` interface."
        );

        static_assert(
            integrator_derived<State, Derivative, Control, Integrator>,
            "The `Integrator` type must derive from"
            "the `OdeIntegratorInterface<State, Derivative, Control>` interface."
        );

        static_assert(
            std::move_constructible<Equation>,
            "The `Equation` type must have a move constructor."
        );

        static_assert(
            std::move_constructible<Integrator>,
            "The `Integrator` type must have a move constructor."
        );
    };

    template<class State, class Derivative, class Control, class Equation, class Integrator>
    requires(
        equation_derived<State, Derivative, Control, Equation> &&
        integrator_derived<State, Derivative, Control, Integrator> &&
        std::move_constructible<Equation> &&
        std::move_constructible<Integrator>
    )
    class ControlledOdeAdapter<State, Derivative, Control, Equation, Integrator> 
        :   public DynamicInterface<StateAndControl<State, Control>> {
    public:
        _XCHILD_NAME(ControlledOdeAdapter)

        using TotalState = StateAndControl<State, Control>;

        ControlledOdeAdapter(Equation equation, Integrator integrator)
            :   m_equation(std::move(equation)),
                m_integrator(std::move(integrator)) {}

        void simulate_step_inplace(
            simulation::Timestamp timestamp,
            TotalState& state,
            simulation::Duration delta_time) const override
        {
            LOG_METHOD();

            this->check_delta_time(delta_time);

            m_integrator.integrate_inplace(
                timestamp,
                timestamp + delta_time,
                state.state,
                state.control,
                m_equation,
                delta_time
            );
        }

    private:
        Equation m_equation;
        Integrator m_integrator;
    };
}
