#pragma once 

#include <algorithm>
#include <concepts>

#include "../../../vulkan_self/logger/logger_header.h"
#include "../clock.h"
#include "ode_equation_interface.h"
#include "ode_integrator_interface.h"

namespace celeris {
    template<class State, class Derivative>
    concept Rk2Compatible = requires(
        State& current_state,
        const State& state,
        const Derivative& k1,
        const Derivative& k2,
        double dt)
    {
        { state + dt * k1 } -> std::same_as<State>;
        { current_state += dt * (k1 + k2) } -> std::same_as<State&>;
    };

    template<class State, class Derivative, class Control>
    requires (Rk2Compatible<State, Derivative>)
    class OdeRk2Integrator final : public OdeIntegratorInterface<State, Derivative, Control> {
    public:
        _XCHILD_NAME(OdeRk2Integrator)

        void integrate_inplace(
            simulation::Timestamp time_start,
            simulation::Timestamp time_end,
            State& initial_state,
            const Control& control,
            const OdeEquationInterface<State, Derivative, Control>& equation,
            simulation::Duration delta_time) const override
        {
            LOG_METHOD();

            logger().check(delta_time > simulation::Duration::zero(), "`delta_time` must be positive");
            logger().check(time_end >= time_start, "`time_end` must not precede `time_start`");
            
            simulation::Timestamp current_timestamp = time_start;
            State& current_state = initial_state;

            while (current_timestamp < time_end) {
                const simulation::Timestamp next_timestamp = std::min(current_timestamp + delta_time, time_end);
                const simulation::Duration duration = next_timestamp - current_timestamp;
                const double duration_seconds = simulation::to_seconds(duration);
                
                const Derivative derivative0 = equation.calculate_derivative(current_timestamp, current_state, control);
                
                const State estimated_state1 = current_state + duration_seconds * derivative0;
                const Derivative derivative1 = equation.calculate_derivative(next_timestamp, estimated_state1, control);

                current_state += duration_seconds / 2.0 * (derivative0 + derivative1);

                current_timestamp = next_timestamp;
            }
        }
    };
}
