#include "fixed_offset_timestamp_mapper.h"

namespace celeris {
    FixedOffsetTimestampMapper::FixedOffsetTimestampMapper(
        std::int64_t src_timestamp_origin_ns, 
        simulation::Timestamp dst_timestamp_origin)
        :   m_src_timestamp_origin_ns(src_timestamp_origin_ns),
            m_dst_timestamp_origin(dst_timestamp_origin) {}

    [[nodiscard]]
    std::optional<simulation::Timestamp> FixedOffsetTimestampMapper::map(
        std::int64_t timestamp_ns, 
        FixedOffsetTimestampMapper::time_point received_at)
    {
        LOG_METHOD();

        std::int64_t duration_nano = timestamp_ns - m_src_timestamp_origin_ns;
        simulation::Duration duration{
            std::chrono::duration<std::int64_t, std::nano>(duration_nano)
        };

        return m_dst_timestamp_origin + duration;
    }
}
