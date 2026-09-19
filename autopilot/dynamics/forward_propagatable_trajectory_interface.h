#pragma once

#include "../../vulkan_self/logger/logger_header.h"
#include "trajectory_interface.h"
#include "clock.h"

namespace celeris {
    template<class Y>
    class ForwardPropagatableTrajectoryInterface : public virtual TrajectoryInterface<Y> {
    public:
        _XCHILD_NAME(ForwardPropagatableTrajectoryInterface);

        using Timestamp = typename TrajectoryInterface<Y>::Timestamp;

        [[nodiscard]]
        virtual Timestamp initial_value_timestamp() const noexcept = 0;

        [[nodiscard]]
        virtual const Y& initial_value() const noexcept = 0; 

        /*
            Распространяет `value`, представляющее значение траектории в `from`,
            до момента `to`.

            После завершения:
            1. `value` содержит значение траектории в `to`

            Предусловия:
            1. `from` <= `to`
            2. `value` = trajectory(`from`). То есть значение `value` равно значению
                траектории в точки `from`
        */
        virtual void propagate_until(Timestamp from, Timestamp to, Y& value) = 0;
    };
}
