#pragma once

#include <iterator>
#include <map>

#include "../../vulkan_self/logger/logger_header.h"
#include "cached_trajectory_interface.h"
#include "clock.h"

namespace celeris {
    template<class Y>
    class MapCachedTrajectory : public CachedTrajectoryInterface<Y> {
    public:
        _XCHILD_NAME(MapCachedTrajectory);

        using Timestamp = typename CachedTrajectoryInterface<Y>::Timestamp;

        MapCachedTrajectory() = default;

    protected:
        Y& store_value_unchecked_mutable(Timestamp timestamp, Y&& value) override {
            LOG_METHOD();

            auto [it, inserted] = m_trajectory_cache.insert_or_assign(timestamp, std::move(value));

            return it->second;
        }

        Y* invalidate_cache_after(Timestamp start_timestamp, bool include_start_timestamp) override {
            LOG_METHOD();

            if (include_start_timestamp) {
                auto erase_begin = m_trajectory_cache.lower_bound(start_timestamp);

                m_trajectory_cache.erase(
                    erase_begin,
                    m_trajectory_cache.end()
                );

                return nullptr;
            }

            auto erase_begin = m_trajectory_cache.upper_bound(timestamp);
            Y* value_at_timestamp = nullptr;

            if (erase_begin != m_trajectory_cache.begin()) {
                auto candidate = std::prev(erase_begin);

                if (candidate->first == timestamp) {
                    value_at_timestamp = &candidate->second;
                }
            }

            m_trajectory_cache.erase(
                erase_begin,
                m_trajectory_cache.end()
            );

            return value_at_timestamp;
        }

    private:
        std::map<Timestamp, Y> m_trajectory_cache;
    };
}
