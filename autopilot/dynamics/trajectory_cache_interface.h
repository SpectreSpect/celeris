#pragma once

#include <functional>
#include <optional>

#include "../../vulkan_self/logger/logger_header.h"
#include "clock.h"

namespace celeris {
    class TimestampBound;

    template<class Y>
    class TrajectoryCacheInterface {
    public:
        _XPARENT_NAME(TrajectoryCacheInterface);

        using Timestamp = simulation::Timestamp;

    private:
        template<class ValueType>
        struct CacheEntryRef {
            Timestamp timestamp;
            std::reference_wrapper<ValueType> value;
        };

    public:
        using CacheEntryRefMutable = CacheEntryRef<Y>;
        using CacheEntryRefConst = CacheEntryRef<const Y>;

        virtual ~TrajectoryCacheInterface() = default;

        virtual Y& store(Timestamp timestamp, Y value) = 0;
        virtual void invalidate(TimestampBound left_bound, TimestampBound right_bound) = 0;

        virtual std::optional<CacheEntryRefConst> find_cached_value_at_or_before(Timestamp timestamp) const = 0;
        virtual std::optional<CacheEntryRefMutable> find_cached_value_at_or_before(Timestamp timestamp) = 0;
    };
}
