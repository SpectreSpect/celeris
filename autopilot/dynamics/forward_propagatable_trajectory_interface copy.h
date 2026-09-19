#pragma once

#include <utility>
#include <optional>
#include <functional>

#include "../../vulkan_self/logger/logger_header.h"
#include "cached_trajectory_interface.h"
#include "clock.h"

namespace celeris {
    template<class Y>
    class ForwardPropagatableTrajectoryInterface : public CachedTrajectoryInterface<Y> {
    public:
        _XCHILD_NAME(ForwardPropagatableTrajectoryInterface);

        using Timestamp = typename CachedTrajectoryInterface<Y>::Timestamp;

        template<class ValueType>
        struct CacheEntryRef {
            Timestamp timestamp;
            std::reference_wrapper<ValueType> value;
        };

        using CacheEntryRefMutable = CacheEntryRef<Y>;
        using CacheEntryRefConst = CacheEntryRef<const Y>;

        [[nodiscard]]
        virtual Timestamp initial_value_timestamp() const noexcept = 0;

        [[nodiscard]]
        virtual const Y& initial_value() const noexcept = 0; 

        /*
            Распространяет `value`, представляющее значение траектории в `from`,
            до момента `to`.

            После завершения:
            1. `value` содержит значение траектории в `to`
            2. Реализация может сохранить промежуточные значения только
                в открытом интервале (`from`, `to`)
            3. Значения непосредственно в `from` и `to` не сохраняются

            Предусловия:
            1. `from` <= `to`
            2. В кэше отсутствуют значения из (`from`, `to`]
            3. `value` = trajectory(`from`). То есть значение `value` равно значению
                траектории в точки `from`. При этом не обязательно, чтобы в этой точки
                это значение было закэшированно. 
        */
        virtual void propagate_and_cache_until(Timestamp from, Timestamp to, Y& value) = 0;

        // Специально не делаем функцию const
        [[nodiscard]]
        std::optional<CacheEntryRefConst> find_cached_value_at_or_before(Timestamp timestamp) {
            LOG_METHOD();

            std::optional<CacheEntryRefMutable> entry = find_cached_value_at_or_before_mutable(timestamp);

            if (entry.has_value()) {
                return CacheEntryRefConst{
                    .timestamp = entry->timestamp,
                    .value = std::as_const(entry->value.get())
                };
            }

            return std::nullopt;
        }
        
    protected:
        /*
            Постусловие:
                Функция в процессе поиска не должна менять структуру или содержимое кэша.
        */
        [[nodiscard]]
        virtual std::optional<CacheEntryRefMutable> find_cached_value_at_or_before_mutable(Timestamp timestamp) = 0;

        Y& define_cached_value_at_mutable(Timestamp timestamp) override {
            LOG_METHOD();

            logger().check(
                timestamp >= initial_value_timestamp(),
                "`timestamp` cannot precede the initial state."
            );

            std::optional<CacheEntryRefMutable> cache_opt = find_cached_value_at_or_before_mutable(timestamp);

            const Timestamp previous_timestamp = cache_opt.has_value() ?
                cache_opt->timestamp : initial_value_timestamp();

            Y* previous_value_ptr = cache_opt.has_value() ? &cache_opt->value.get() : nullptr;

            if (!cache_opt.has_value()) {
                // Начальное значение ещё не помещено в кэш
                previous_value_ptr = &this->store_value_unchecked_mutable(
                    initial_value_timestamp(), 
                    initial_value()
                );
            }

            if (previous_timestamp == timestamp) {
                return *previous_value_ptr;
            }
            
            Y previous_value_copy = *previous_value_ptr;

            propagate_and_cache_until(previous_timestamp, timestamp, previous_value_copy);

            return this->store_value_unchecked_mutable(timestamp, std::move(previous_value_copy));
        }
    };
}
