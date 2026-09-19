#pragma once

#include <utility>

#include "../../vulkan_self/logger/logger_header.h"
#include "forward_propagatable_trajectory_interface.h"
#include "clock.h"

namespace celeris {
    template<class Y>
    class ForwardPropagatableTrajectoryBase : ForwardPropagatableTrajectoryInterface<Y> {
    public:
        _XCHILD_NAME(ForwardPropagatableTrajectoryBase);

        using Timestamp = typename ForwardPropagatableTrajectoryInterface<Y>::Timestamp;

        ForwardPropagatableTrajectoryBase(
            Timestamp initial_value_timestamp,
            Y initial_value
        )   :   m_initial_value_timestamp(initial_value_timestamp),
                m_initial_value(std::move(initial_value)) {}

        ForwardPropagatableTrajectoryBase(const ForwardPropagatableTrajectoryBase&) = default;
        ForwardPropagatableTrajectoryBase& operator=(const ForwardPropagatableTrajectoryBase&) = default;

        ForwardPropagatableTrajectoryBase(ForwardPropagatableTrajectoryBase&&) noexcept = default;
        ForwardPropagatableTrajectoryBase& operator=(ForwardPropagatableTrajectoryBase&&) noexcept = default;

        [[nodiscard]]
        Timestamp initial_value_timestamp() const noexcept override {
            return m_initial_value_timestamp;
        }

        [[nodiscard]]
        const Y& initial_value() const noexcept override {
            return m_initial_value;
        }

        Y define_trajectory(Timestamp timestamp) override {
            LOG_METHOD();

            logger().check(
                timestamp >= initial_value_timestamp(),
                "`timestamp` cannot precede the initial state."
            );

            Y propagatable_value = initial_value();
            propagate_until(initial_value_timestamp(), timestamp, propagatable_value);

            return propagatable_value;
        }

    private:
        Timestamp m_initial_value_timestamp;
        Y m_initial_value;
    };
}
