#pragma once

#include <optional>
#include <utility>
#include <map>

#include "trajectory_cache_interface.h"
#include "timestamp_bound.h"
#include "clock.h"

namespace celeris {
    /*
        Интерфейсы, как я понимаю, желательно всегда наследовать виртуально?
        Не представляю ситуации, когда может понадобиться две их "копии"...
    */
    template<class Y>
    class MapTrajectoryCache : public virtual TrajectoryCacheInterface<Y> {
    public:
        _XCHILD_NAME(MapTrajectoryCache);

        // И всё-таки я бы хотел, чтобы ты поподробнее рассказал, зачем здесь нужен typename...
        using Timestamp = typename TrajectoryCacheInterface<Y>::Timestamp;
        using CacheEntryRefMutable = typename TrajectoryCacheInterface<Y>::CacheEntryRefMutable;
        using CacheEntryRefConst = typename TrajectoryCacheInterface<Y>::CacheEntryRefConst;

        MapTrajectoryCache() = default;

        MapTrajectoryCache(const MapTrajectoryCache&) = default;
        MapTrajectoryCache& operator=(const MapTrajectoryCache&) = default;

        MapTrajectoryCache(MapTrajectoryCache&&) noexcept = default;
        MapTrajectoryCache& operator=(MapTrajectoryCache&&) noexcept = default;

        Y& store(Timestamp timestamp, Y value) override {
            LOG_METHOD();

            auto [it, inserted] = m_cache.insert_or_assign(timestamp, std::move(value));

            return it->second;
        }

        void invalidate(TimestampBound left_bound, TimestampBound right_bound) override {
            LOG_METHOD();

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
