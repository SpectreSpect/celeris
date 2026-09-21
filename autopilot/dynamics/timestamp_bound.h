#pragma once

#include <concepts>
#include <ranges>

#include "../../vulkan_self/logger/logger_header.h"
#include "clock.h"

namespace celeris {
    template<class Container, class Key>
    concept bound_searchable_range =
        std::ranges::forward_range<Container> &&
        std::ranges::common_range<Container> &&
        requires(Container& container, const Key& key) {
            { container.lower_bound(key) }
                -> std::same_as<std::ranges::iterator_t<Container>>;

            { container.upper_bound(key) }
                -> std::same_as<std::ranges::iterator_t<Container>>;
        };
    
    enum BoundValueType {
        NEG_INFINITY,
        NUMERICAL_VALUE,
        PLUS_INFINITY
    };

    class TimestampBound {
    public:
        _XPARENT_NAME(TimestampBound);

        using Timestamp = simulation::Timestamp;
        
        [[nodiscard]]
        BoundValueType bound_value_type() const noexcept;

        [[nodiscard]]
        bool is_include() const noexcept;

        [[nodiscard]]
        bool is_left_bound() const noexcept;

        [[nodiscard]]
        Timestamp operator*() const;

        [[nodiscard]]
        static TimestampBound left_include(Timestamp timestamp);

        [[nodiscard]]
        static TimestampBound left_exclude(Timestamp timestamp);

        [[nodiscard]]
        static TimestampBound right_include(Timestamp timestamp);

        [[nodiscard]]
        static TimestampBound right_exclude(Timestamp timestamp);

        [[nodiscard]]
        static TimestampBound plus_inf();

        [[nodiscard]]
        static TimestampBound neg_inf();

        template<class Container>
        requires(bound_searchable_range<Container, Timestamp>)
        static std::ranges::iterator_t<Container> to_iterator(Container& container, TimestampBound bound) {
            LOG_NAMED("TimestampBound");

            if (bound.bound_value_type() != BoundValueType::NUMERICAL_VALUE) {
                return bound.bound_value_type() == BoundValueType::NEG_INFINITY ? 
                        std::ranges::begin(container) : std::ranges::end(container);
            } else {
                Timestamp timestamp_bound = *bound;

                if (bound.is_left_bound()) {
                    return bound.is_include() ? 
                        container.lower_bound(timestamp_bound) : 
                        container.upper_bound(timestamp_bound);
                } else {
                    return bound.is_include() ?
                        container.upper_bound(timestamp_bound) :
                        container.lower_bound(timestamp_bound);
                }
            }
        }
    
    private:
        TimestampBound(
            Timestamp value,
            bool is_left_bound,
            bool include, 
            BoundValueType bound_value_type
        );

        TimestampBound(BoundValueType bound_value_type);

    private:
        Timestamp m_bound;
        bool m_is_left_bound = true;
        bool m_include = true;
        BoundValueType m_bound_value_type = BoundValueType::NUMERICAL_VALUE;
    };
}
