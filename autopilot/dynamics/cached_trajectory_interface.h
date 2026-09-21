#pragma once

#include <utility>

#include "../../vulkan_self/logger/logger_header.h"
#include "trajectory_interface.h"
#include "timestamp_bound.h"
#include "clock.h"

namespace celeris {
    template<class Y>
    class CachedTrajectoryInterface : public virtual TrajectoryInterface<Y> {
    public:
        _XCHILD_NAME(CachedTrajectoryInterface);

        virtual ~CachedTrajectoryInterface() = default;

        using Timestamp = typename TrajectoryInterface<Y>::Timestamp;
    
        const Y& store_unchecked(Timestamp timestamp, Y value) {
            LOG_METHOD();

            return store_uncheked_mutable(timestamp, std::move(value));
        }

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
            Сохраняет или заменяет значение в кэше в точке `timestamp`
            значением `value`.
            
            Возвращает ссылку на сохранённое после работы функции значение
            `value` в точке `timestamp` внутри кэша.

            ФУНЦИЯ НЕ БЕЗОПАСНА (безопасная версия функции store_mutable(Timestamp timestamp, Y value)):
                Если до сохранения было:
                this->define_trajectory(`timestamp`) != `value`
                
                То после сохранения весь кэш с `cache_timestamp` > `timestamp` станет
                НЕКОРРЕКТНЫМ!!! 
                
                Его нужно будет очищать:
                invalidate_cache(TimestampBound::left_exclude(timestamp), TimestampBound::plus_inf());
            
            КОГДА КЭШ ОЧИЩАТЬ НЕ НУЖНО:
                Если до сохранения было:
                this->define_trajectory(`timestamp`) == `value`

                То после сохранения весь кэш с `cache_timestamp` > `timestamp`
                останется корректным.
        */
        virtual Y& store_uncheked_mutable(Timestamp timestamp, Y value) = 0;

        /*
            Сохраняет или заменяет значение в кэше в точке `timestamp`
            значением `value`. После сохранения стирает весь кэш после
            точки `timestamp`.
            
            Возвращает ссылку на сохранённое после работы функции значение
            `value` в точке `timestamp` внутри кэша.

            Безопасная версия функции store_uncheked_mutable()
        */
        Y& store_mutable(Timestamp timestamp, Y value) {
            LOG_METHOD();

            Y& cached_value = store_uncheked_mutable(timestamp, std::move(value));

            // Исключаем `cached_value` из диапазона
            invalidate_cache(TimestampBound::left_exclude(timestamp), TimestampBound::plus_inf());

            return cached_value;
        }

        /*
            Определяет значение траектории в момент времени `timestamp` 
            и кэширует его.

            Возвращает ссылку на закэшированное значение.
        */
        virtual Y& define_cached_value_at_mutable(Timestamp timestamp) = 0;
    };
}
