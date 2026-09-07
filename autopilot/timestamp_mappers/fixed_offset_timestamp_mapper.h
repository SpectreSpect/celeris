#pragma once

#include "../../vulkan_self/logger/logger_header.h"
#include "timestamp_mapper.h"
#include "../dynamics/clock.h"

namespace celeris {
    class FixedOffsetTimestampMapper final : public TimestampMapper {
    public:
        _XCHILD_NAME(FixedOffsetTimestampMapper);

        FixedOffsetTimestampMapper(
            std::int64_t src_timestamp_origin_ns, 
            simulation::Timestamp dst_timestamp_origin
        );

        [[nodiscard]]
        std::optional<simulation::Timestamp> map(std::int64_t timestamp_ns, time_point received_at) override;

    private:
        std::int64_t m_src_timestamp_origin_ns;
        simulation::Timestamp m_dst_timestamp_origin;
    };
}
