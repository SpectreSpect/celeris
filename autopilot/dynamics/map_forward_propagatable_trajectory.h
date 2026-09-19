#pragma once

#include <optional>
#include <iterator>
#include <utility>
#include <map>

#include "../../vulkan_self/logger/logger_header.h"
#include "map_cached_trajectory.h"
#include "clock.h"

namespace celeris {
    template<class Y>
    class MapForwardPropagatableTrajectory 
        :   public MapCachedTrajectory<Y>, public ForwardPropagatableTrajectoryInterface<Y> {
    public:
        _XCHILD_NAME(MapForwardPropagatableTrajectory);

        using Timestamp = typename MapCachedTrajectory<Y>::Timestamp;
        using CacheEntryRefMutable = typename ForwardPropagatableTrajectoryInterface<Y>::CacheEntryRefMutable;
        using CacheEntryRefConst = typename ForwardPropagatableTrajectoryInterface<Y>::CacheEntryRefConst;

        explicit MapForwardPropagatableTrajectory(
            Timestamp initial_value_timestamp,
            Y initial_value)
            :   m_initial_value_timestamp(initial_value_timestamp),
                m_initial_value(std::move(initial_value)) {}

        [[nodiscard]]
        Timestamp initial_value_timestamp() const noexcept override {
            return m_initial_value_timestamp;
        }

        [[nodiscard]]
        const Y& initial_value() const noexcept override {
            return m_initial_value;
        }
    
    protected:
        /*
            Постусловие:
                Функция в процессе поиска не должна менять структуру или содержимое кэша.
        */
        [[nodiscard]]
        std::optional<CacheEntryRefMutable> find_cached_value_at_or_before_mutable(Timestamp timestamp) override {
            LOG_METHOD();

            logger().check(
                timestamp >= initial_value_timestamp(),
                "`timestamp` cannot precede the initial state."
            );

            // Здесь же тоже нужно this->?
            std::map<Timestamp, Y>& m = this->m_trajectory_cache;

            // Через upper_bound() проще как будто
            auto it = m.upper_bound(timestamp);

            if (it == m.begin()) {
                return std::nullopt;
            }

            auto prev = std::prev(it);

            return CacheEntryRefMutable{
                .timestamp = prev->first,
                .value = prev->second
            };
        }

    private:
        Timestamp m_initial_value_timestamp;
        Y m_initial_value;
    };
}
