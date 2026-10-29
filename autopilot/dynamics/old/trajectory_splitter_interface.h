#pragma once

#include <vector>

#include "../../vulkan_self/logger/logger_header.h"
#include "clock.h"

namespace celeris {
    template<class Y>
    class TrajectorySplitterInterface {
    public:
        _XPARENT_NAME(TrajectorySplitterInterface);

        using Timestamp = simulation::Timestamp;
        using Duration = simulation::Duration;

        virtual ~TrajectorySplitterInterface() = default;

        virtual std::vector<Timestamp> split(Timestamp from, Timestamp to, const Y& value) {
            LOG_METHOD();

            logger().check(from <= to, "`from` must preceed `to` or be equal to it.");

            std::vector<Timestamp> split_points = split_protected(from, to, value);

            // Была бы в C++ импликация...
            if (from == to) {
                logger().check(
                    split_points.empty(),
                    "If `from` == `to`, the number of `split_points` must be zero."
                )
            }
            
            bool is_consistently = true;
            Timestamp prev = from;
            for (Timestamp t : split_points) {
                if (prev >= t) {
                    is_consistently = false;
                    break;
                }

                prev = t;
            }

            is_consistently = is_consistently ? prev < to : is_consistently;
            
            /*
                Использовал `is_consistently` для оптимизации, чтобы
                не вызывать логгер в цикле.
            */
            logger().check(
                is_consistently, 
                "The points must be arranged sequentially in ascending order:"
                "`from` < `t1` < `t2` < `...` < `from`"
            );

            return split_points;
        }

        protected:
        // Возвращает точки разреза интервала
        virtual std::vector<Timestamp> split_protected(Timestamp from, Timestamp to, const Y& value) = 0;
    };
}
