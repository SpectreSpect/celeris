#pragma once

#include <optional>
#include <iterator>
#include <utility>
#include <memory>
#include <map>

#include "trajectory_cache_interface.h"
#include "timestamp_bound.h"
#include "clock.h"

namespace celeris {
    template<class Y>
    class MapTrajectoryCache : public virtual TrajectoryCacheInterface<Y> {
    public:
        _XCHILD_NAME(MapTrajectoryCache);

        using Timestamp = typename TrajectoryCacheInterface<Y>::Timestamp;
        using CacheEntryRefMutable = typename TrajectoryCacheInterface<Y>::CacheEntryRefMutable;
        using CacheEntryRefConst = typename TrajectoryCacheInterface<Y>::CacheEntryRefConst;

        MapTrajectoryCache() = default;

        MapTrajectoryCache(const MapTrajectoryCache&) = delete;
        MapTrajectoryCache& operator=(const MapTrajectoryCache&) = delete;

        MapTrajectoryCache(MapTrajectoryCache&&) = default;
        MapTrajectoryCache& operator=(MapTrajectoryCache&&) = default;

        Y& store(Timestamp timestamp, Y value) override {
            LOG_METHOD();

            auto [it, inserted] = m_cache.insert_or_assign(timestamp, std::move(value));

            return it->second;
        }

        void invalidate(TimestampBound left_bound, TimestampBound right_bound) override {
            LOG_METHOD();

            bool both_intervals_have_numerical_values = 
                left_bound.bound_value_type() == 0 && right_bound.bound_value_type() == 0;
            
            if (both_intervals_have_numerical_values) {
                bool is_entire_interval_erasing = left_bound.is_include() && right_bound.is_include();

                if (*left_bound == *right_bound && !is_entire_interval_erasing) {
                    return;
                }
            }

            m_cache.erase(
                to_iterator(left_bound),
                to_iterator(right_bound)
            );
        }

        std::optional<CacheEntryRefConst> find_cached_value_at_or_before(Timestamp timestamp) const override {
            LOG_METHOD();

            auto it = m_cache.upper_bound(timestamp);

            if (it == m_cache.begin()) {
                return std::nullopt;
            }

            auto prev = std::prev(it);

            return CacheEntryRefConst{
                .timestamp = prev->first,
                .value = prev->second
            };
        }

        std::optional<CacheEntryRefMutable> find_cached_value_at_or_before(Timestamp timestamp) override {
            LOG_METHOD();

            std::optional<CacheEntryRefConst> entry_const = 
                std::as_const(*this).find_cached_value_at_or_before(timestamp);

            if (!entry_const.has_value()) {
                return std::nullopt;
            }

            return CacheEntryRefMutable{
                .timestamp = entry_const->timestamp,
                .value = const_cast<Y&>(entry_const->value.get())
            }; 
        }
    
    private:
        std::map<Timestamp, Y> m_cache;
    
    private:
        std::map<Timestamp, Y>::iterator to_iterator(TimestampBound bound) {
            LOG_METHOD();

            return TimestampBound::to_iterator(m_cache, bound);
        }
    };
}
