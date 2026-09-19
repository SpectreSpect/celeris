#pragma once

#include <utility>
#include <vector>
#include <memory>

#include "../../vulkan_self/logger/logger_header.h"
#include "forward_propagatable_trajectory_base.h"
#include "trajectory_splitter_interface.h"
#include "cached_trajectory_interface.h"
#include "trajectory_cache_interface.h"
#include "clock.h"

namespace celeris {
    template<class Y>
    class CachedForwardPropagatableTrajectory
        :   public ForwardPropagatableTrajectoryBase<Y>, 
            public virtual CachedTrajectoryInterface<Y> {
    public:
        _XCHILD_NAME(CachedForwardPropagatableTrajectory);

        CachedForwardPropagatableTrajectory(
            Timestamp initial_value_timestamp,
            Y initial_value,
            std::unique_ptr<TrajectoryCacheInterface<Y>> cache_component,
            std::unique_ptr<TrajectorySplitterInterface<Y>> splitter
        )   :   ForwardPropagatableTrajectoryBase<Y>(
                    initial_value_timestamp,
                    std::move(initial_value)
                ),
                m_cache_component(std::move(cache_component)),
                m_splitter(std::move(splitter)) {}

        CachedForwardPropagatableTrajectory(const CachedForwardPropagatableTrajectory&) = default;
        CachedForwardPropagatableTrajectory& operator=(const CachedForwardPropagatableTrajectory&) = default;

        CachedForwardPropagatableTrajectory(CachedForwardPropagatableTrajectory&&) noexcept = default;
        CachedForwardPropagatableTrajectory& operator=(CachedForwardPropagatableTrajectory&&) noexcept = default;
    
        Y define_trajectory(Timestamp timestamp) override {
            LOG_METHOD();

            logger().check(
                timestamp >= this->initial_value_timestamp(),
                "`timestamp` cannot precede the initial value."
            );

            return define_cached_value_at_mutable(timestamp);
        }

        void propagate_and_cache_until(Timestamp from, Timestamp to, Y& value) {
            LOG_METHOD();

            logger().check(
                from >= this->initial_value_timestamp(),
                "`from` cannot preceed the initial value timestamp."
            );
            logger().check(
                to >= this->initial_value_timestamp(),
                "`from` cannot preceed the initial value timestamp."
            );
            logger().check(from <= to, "`from` must preceed `to`.");
            logger().check(m_splitter != nullptr, "`m_splitter` is null.");

            std::vector<Timestamp> split_points = m_splitter->split(from, to, value);

            // В точке `from` не сохраняем

            Timestamp prev = from;
            for (Timestamp t : split_points) {
                propagate_until(prev, t, value);
                store_mutable(t, value);
                
                prev = t;
            }

            propagate_until(prev, to, value);
            
            // В точке `to` не сохраняем
        }

    protected:
        /*
            Сохраняет или заменяет значение в кэше точке `timestamp`
            значением `value`.
            
            Возвращает ссылку на сохранённое после работы функции значение
            `value` в точке `timestamp` внутри кэша.
        */
        Y& store_mutable(Timestamp timestamp, Y value) override {
            LOG_METHOD();
            
            logger().check(m_cache_component != nullptr, "`m_cache_component` is null.");
            logger().check(
                timestamp > this->initial_value_timestamp(), 
                "`timestamp` cannot preceed the initial value timestamp or be equal to it."
            );

            return m_cache_component->store(timestamp, std::move(value));
        }

        /*
            Стирает кэш в указанном диапазоне.
        */
        void invalidate_cache(TimestampBound left_bound, TimestampBound right_bound) override {
            LOG_METHOD();

            logger().check(m_cache_component != nullptr, "`m_cache_component` is null.");

            m_cache_component->invalidate(left_bound, right_bound);
        }

        /*
            Определяет значение траектории в момент времени `timestamp` 
            и кэширует его.

            Возвращает ссылку на закэшированное значение.
        */
        Y& define_cached_value_at_mutable(Timestamp timestamp) override {
            LOG_METHOD();

            logger().check(
                timestamp >= this->initial_value_timestamp(),
                "`timestamp` cannot precede the initial value."
            );

            std::optional<CacheEntryRefMutable> cache_opt = m_cache_component->find_cached_value_at_or_before(timestamp);

            const Timestamp previous_timestamp =
                cache_opt.has_value() ?
                cache_opt->timestamp :
                this->initial_value_timestamp();

            Y* previous_value_ptr = cache_opt.has_value() ? &cache_opt->value.get() : nullptr;

            if (!cache_opt.has_value()) {
                // Начальное значение ещё не помещено в кэш
                previous_value_ptr = &m_cache_component->store(
                    this->initial_value_timestamp(), 
                    this->initial_value()
                );
            }

            if (previous_timestamp == timestamp) {
                return *previous_value_ptr;
            }
            
            Y previous_value_copy = *previous_value_ptr;

            propagate_and_cache_until(previous_timestamp, timestamp, previous_value_copy);

            return store_mutable(timestamp, std::move(previous_value_copy));
        }
    
    private:
        std::unique_ptr<TrajectoryCacheInterface<Y>> m_cache_component;
        std::unique_ptr<TrajectorySplitterInterface<Y>> m_splitter;
    };
}
