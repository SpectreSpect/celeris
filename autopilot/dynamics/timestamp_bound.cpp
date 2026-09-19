#include "timestamp_bound.h"

namespace celeris {
    [[nodiscard]]
    char TimestampBound::is_infinity() const noexcept {
        return m_is_infinity;
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
            is_infinity() == 0,
            "The bound is infinite — it is impossible to determine its numerical value."
        );

        return m_bound;
    }

    [[nodiscard]]
    TimestampBound TimestampBound::left_include(Timestamp timestamp) {
        return TimestampBound(timestamp, true, true, 0);
    }

    [[nodiscard]]
    TimestampBound TimestampBound::left_exclude(Timestamp timestamp) {
        return TimestampBound(timestamp, true, false, 0);
    }

    [[nodiscard]]
    TimestampBound TimestampBound::right_include(Timestamp timestamp) {
        return TimestampBound(timestamp, false, true, 0);
    }

    [[nodiscard]]
    TimestampBound TimestampBound::right_exclude(Timestamp timestamp) {
        return TimestampBound(timestamp, false, false, 0);
    }

    [[nodiscard]]
    TimestampBound TimestampBound::plus_inf() {
        return TimestampBound(1);
    }

    [[nodiscard]]
    TimestampBound TimestampBound::neg_inf() {
        return TimestampBound(-1);
    }

    [[nodiscard]]
    TimestampBound::TimestampBound(
        Timestamp bound,
        bool is_left_bound,
        bool include,    
        char is_inifinity) 
        :   m_bound(bound),
            m_is_left_bound(is_left_bound),
            m_include(include),
            m_is_infinity(is_inifinity) {}

    [[nodiscard]]
    TimestampBound::TimestampBound(char is_inifinity) 
        :   m_is_infinity(is_inifinity) {}
}
