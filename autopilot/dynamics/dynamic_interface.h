#pragma once

#include <algorithm>

#include "../../vulkan_self/logger/logger_header.h"

template<class State>
class DynamicInterface {
public:
    _XPARENT_NAME(DynamicInterface)

    virtual ~DynamicInterface() noexcept = default;

    virtual State& simulate_step_inplace(State& state, double dt) const = 0;

    [[nodiscard]]
    State simulate_step(State state, double dt) const {
        LOG_METHOD();

        simulate_step_inplace(state, dt);
        return state;
    }

    State& simulate_dynamic_inplace(State& state, double simulation_time, double dt) const {
        LOG_METHOD();

        logger().check(simulation_time >= 0.0)
            << "`simulation_time` must be non-negative.";

        logger().check(dt > 0.0)
            << "`dt` must be positive.";

        for (double t = 0.0; t < simulation_time;) {
            const double step_dt = std::min(dt, simulation_time - t);

            simulate_step_inplace(state, step_dt);

            t += step_dt;
        }

        return state;
    }

    [[nodiscard]]
    State simulate_dynamic(State state, double simulation_time, double dt) const {
        LOG_METHOD();

        simulate_dynamic_inplace(state, simulation_time, dt);
        return state;
    }

protected:
    DynamicInterface() = default;
    
    DynamicInterface(const DynamicInterface&) = default;
    DynamicInterface& operator=(const DynamicInterface&) = default;

    DynamicInterface(DynamicInterface&&) noexcept = default;
    DynamicInterface& operator=(DynamicInterface&&) noexcept = default;
};
