#pragma once

#include "../../vulkan_self/logger/logger_header.h"
#include "trajectory_interface.h"
#include "timestamp_bound.h"
#include "clock.h"

namespace celeris {
    template<class Y>
    class CachedTrajectoryInterface : public virtual TrajectoryInterface<Y> {
    public:
        _XCHILD_NAME(CachedTrajectoryInterface);

        using Timestamp = typename TrajectoryInterface<Y>::Timestamp;
    
        const Y& store(Timestamp timestamp, Y value) {
            LOG_METHOD();

            return store_mutable(timestamp, std::move(value));
        }

        const Y& define_cached_value_at(Timestamp timestamp) {
            LOG_METHOD();

            return define_cached_value_at_mutable(timestamp);
        }

        void materialize_cache_at(Timestamp timestamp) {
            LOG_METHOD();

            define_cached_value_at_mutable(timestamp);
        }

        /*
            Стирает кэш в указанном диапазоне.
        */
        virtual void invalidate_cache(TimestampBound left_bound, TimestampBound right_bound) = 0;

    protected:
        /*
            Сохраняет или заменяет значение в кэше точке `timestamp`
            значением `value`.
            
            Возвращает ссылку на сохранённое после работы функции значение
            `value` в точке `timestamp` внутри кэша.
        */
        virtual Y& store_mutable(Timestamp timestamp, Y value) = 0;

        /*
            Определяет значение траектории в момент времени `timestamp` 
            и кэширует его.

            Возвращает ссылку на закэшированное значение.
        */
        virtual Y& define_cached_value_at_mutable(Timestamp timestamp) = 0;
    };
}
