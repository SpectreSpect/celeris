#pragma once

#include <algorithm>

#include "../../../vulkan_self/logger/logger_header.h"
#include "../clock.h"

namespace celeris {
    template<class State>
    class DynamicInterface {
    public:
        _XPARENT_NAME(DynamicInterface)

        virtual ~DynamicInterface() noexcept = default;

        virtual void simulate_step_inplace(simulation::Timestamp timestamp, State& state, simulation::Duration delta_time) const = 0;

        static void check_delta_time(simulation::Duration delta_time) {
            logger().check(delta_time > simulation::Duration::zero())
                << "`delta_time` must be positive.";
        }

        static void check_duration(simulation::Duration duration) {
            logger().check(duration >= simulation::Duration::zero())
                << "`duration` must be non-negative.";
        }

        [[nodiscard]]
        State simulate_step(simulation::Timestamp timestamp, State state, simulation::Duration delta_time) const {
            LOG_METHOD();

            check_delta_time(delta_time);

            simulate_step_inplace(timestamp, state, delta_time);
            return state;
        }

        void simulate_for_inplace(
            simulation::Timestamp timestamp,
            State& state,
            simulation::Duration duration,
            simulation::Duration delta_time) const
        {
            LOG_METHOD();
            
            check_duration(duration);
            check_delta_time(delta_time);

            simulation::Timestamp current_time = timestamp;
            const simulation::Timestamp end_time = current_time + duration;

            while (current_time < end_time) {
                simulation::Timestamp next_timestamp = std::min(current_time + delta_time, end_time);
                const simulation::Duration step_duration = next_timestamp - current_time;

                simulate_step_inplace(current_time, state, step_duration);

                current_time = next_timestamp;
            }
        }

        [[nodiscard]]
        State simulate_for(
            simulation::Timestamp timestamp,
            State state,
            simulation::Duration duration,
            simulation::Duration delta_time) const
        {
            LOG_METHOD();

            check_duration(duration);
            check_delta_time(delta_time);

            simulate_for_inplace(timestamp, state, duration, delta_time);
            return state;
        }
    };
}
