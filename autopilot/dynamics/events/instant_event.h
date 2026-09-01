#pragma once

#include "../clock.h"
#include "../state_estimate.h"
#include "../../../vulkan_self/logger/logger_header.h"

namespace celeris {
    template<class State>
    class InstantEvent {
    public:
        _XPARENT_NAME(InstantEvent);

        InstantEvent(simulation::Timestamp timestamp)
            :   m_timestamp(timestamp) {}

        virtual ~InstantEvent() noexcept = default;

        simulation::Timestamp timestamp() const noexcept {
            return m_timestamp;
        }

        virtual void apply(StateEstimate<State>& state) const = 0;
    
    protected:
        InstantEvent(const InstantEvent&) = default;
        InstantEvent& operator=(const InstantEvent&) = default;

        InstantEvent(InstantEvent&&) noexcept = default;
        InstantEvent& operator=(InstantEvent&&) noexcept = default;

    private:
        simulation::Timestamp m_timestamp;
    };
}
