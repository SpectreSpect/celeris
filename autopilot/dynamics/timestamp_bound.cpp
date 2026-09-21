#include "timestamp_bound.h"

namespace celeris {
    [[nodiscard]]
    BoundValueType TimestampBound::bound_value_type() const noexcept {
        return m_bound_value_type;
    }

    [[nodiscard]]
    bool TimestampBound::is_include() const noexcept {
        return m_include;
    }

    [[nodiscard]]
    bool TimestampBound::is_left_bound() const noexcept {
        return m_is_left_bound;
    }

    [[nodiscard]]
    TimestampBound::Timestamp TimestampBound::operator*() const {
        LOG_METHOD();
        
        logger().check(
            bound_value_type() == 0,
            "The bound is infinite — it is impossible to determine its numerical value."
        );

        return m_bound;
    }

    [[nodiscard]]
    TimestampBound TimestampBound::left_include(Timestamp timestamp) {
        return TimestampBound(timestamp, true, true, BoundValueType::NUMERICAL_VALUE);
    }

    [[nodiscard]]
    TimestampBound TimestampBound::left_exclude(Timestamp timestamp) {
        return TimestampBound(timestamp, true, false, BoundValueType::NUMERICAL_VALUE);
    }

    [[nodiscard]]
    TimestampBound TimestampBound::right_include(Timestamp timestamp) {
        return TimestampBound(timestamp, false, true, BoundValueType::NUMERICAL_VALUE);
    }

    [[nodiscard]]
    TimestampBound TimestampBound::right_exclude(Timestamp timestamp) {
        return TimestampBound(timestamp, false, false, BoundValueType::NUMERICAL_VALUE);
    }

    [[nodiscard]]
    TimestampBound TimestampBound::plus_inf() {
        return TimestampBound(BoundValueType::PLUS_INFINITY);
    }

    [[nodiscard]]
    TimestampBound TimestampBound::neg_inf() {
        return TimestampBound(BoundValueType::NEG_INFINITY);
    }

    [[nodiscard]]
    TimestampBound::TimestampBound(
        Timestamp bound,
        bool is_left_bound,
        bool include,    
        BoundValueType bound_value_type) 
        :   m_bound(bound),
            m_is_left_bound(is_left_bound),
            m_include(include),
            m_bound_value_type(bound_value_type) {}

    [[nodiscard]]
    TimestampBound::TimestampBound(BoundValueType bound_value_type) 
        :   m_bound_value_type(bound_value_type) {}
}
