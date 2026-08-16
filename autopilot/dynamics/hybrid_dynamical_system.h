#pragma once

#include <memory>
#include <deque>
#include <utility>
#include <algorithm>
#include <vector>
#include <map>
#include <chrono>
#include <optional>

#include "../../vulkan_self/logger/logger_header.h"
#include "dynamic_interface.h"

namespace celeris {
    namespace simulation {
        struct Clock {
            using duration = std::chrono::nanoseconds;
            using rep = duration::rep;
            using period = duration::period;
            using time_point = std::chrono::time_point<Clock, duration>;
        };

        using Timestamp = Clock::time_point;
        using Duration = Clock::duration;

        [[nodiscard]]
        constexpr double to_seconds(Duration duration) noexcept {
            return std::chrono::duration<double>{duration}.count();
        }
    }

    template<class State>
    struct StateEstimate {
        State state;
        simulation::Timestamp timestamp;
        // В теории сюда можно добавить информацию о "достоверности" состояния...
    };

    template<class State>
    class InstantEvent {
    public:
        _XPARENT_NAME(InstantEvent);

        virtual ~InstantEvent() noexcept = default;

        virtual simulation::Timestamp timestamp() const noexcept = 0;
        virtual void apply(StateEstimate<State>& state) const = 0;
    };

    template<class State>
    class HybridDynamicalSystem {
    public:
        _XPARENT_NAME(HybridDynamicalSystem);

        using EventPtr = std::unique_ptr<InstantEvent<State>>;
        using EventBucket = std::vector<EventPtr>;

        using Timestamp = simulation::Timestamp;
        using Duration = simulation::Duration;

        /*
            max_integration_step - максимальный шаг интегрирования (delta_time)
            max_history_step - максимальный размер временных промежутков, между которыми сохраняется состояние динамики

            P. S. "Максимальный", потому что размер шага иногда может быть меньше указанного значения.
        */
        explicit HybridDynamicalSystem(
            std::unique_ptr<DynamicInterface<State>> dynamic_model,
            const StateEstimate<State>& initial_state,
            Duration max_integration_step = std::chrono::milliseconds{10},
            Duration max_history_step = std::chrono::milliseconds{100})
            :   m_dynamic_model(std::move(dynamic_model)),
                m_initial_state(initial_state),
                m_max_integration_step(max_integration_step),
                m_max_history_step(max_history_step)
        {
            LOG_METHOD();

            logger().check(m_dynamic_model != nullptr, "`dynamic_model` must not be null.");
            logger().check(
                m_max_integration_step > Duration::zero(),
                "`max_integration_step` must be greater than zero."
            );
            logger().check(
                m_max_history_step > Duration::zero(),
                "`max_history_step` must be greater than zero."
            );

            m_state_history.push_back(m_initial_state);
        }

        const StateEstimate<State>& current_state() const noexcept {
            return m_state_history.back();
        }

        void insert_events(std::vector<EventPtr> events) {
            LOG_METHOD();

            if (events.empty()) {
                return;
            }
            
            for (const EventPtr& event : events) {
                check_event_ptr(event);
            }

            const Timestamp previous_end = current_state().timestamp;

            std::optional<Timestamp> earliest_affected;

            for (EventPtr& event : events) {
                const Timestamp timestamp = event->timestamp();

                if (timestamp <= previous_end && (!earliest_affected || timestamp < *earliest_affected)) {
                    earliest_affected = timestamp;
                }

                insert_event_without_resimulation(std::move(event));
            }

            if (earliest_affected) {
                resimulate_at_and_until(*earliest_affected, previous_end);
            }
        }

        void insert_event(EventPtr event) {
            LOG_METHOD();

            check_event_ptr(event);

            const Timestamp event_timestamp = event->timestamp();
            const Timestamp previous_end = current_state().timestamp;

            insert_event_without_resimulation(std::move(event));

            if (event_timestamp <= previous_end) {
                resimulate_at_and_until(event_timestamp, previous_end);
            }
        }

        const StateEstimate<State>& simulate_for(Duration duration) {
            LOG_METHOD();

            logger().check(m_dynamic_model != nullptr, "`dynamic_model` must not be null.");
            logger().check(duration >= Duration::zero(),
                "`duration` must be non-negative."
            );

            const Timestamp end_timestamp = current_state().timestamp + duration;
            while (current_state().timestamp < end_timestamp) {
                auto next = m_event_history.upper_bound(current_state().timestamp);

                if (next == m_event_history.end() || next->first > end_timestamp) {
                    simulate_without_events_until(end_timestamp);
                    break;
                }

                const Timestamp event_timestamp = next->first;

                simulate_without_events_until(event_timestamp);
                apply_events_to_state(mutable_current_state(), next->second);
            }

            return current_state();
        }

        const StateEstimate<State>& simulate_until(Timestamp timestamp) {
            LOG_METHOD();

            logger().check(m_dynamic_model != nullptr, "`dynamic_model` must not be null.");
            logger().check(timestamp >= current_state().timestamp, "The simulation must simulate dynamics extending into the future.");

            const Duration duration = timestamp - current_state().timestamp;
            return simulate_for(duration);
        }

        const StateEstimate<State>& resimulate_at_and_for(Timestamp start_timestamp, Duration duration) {
            LOG_METHOD();

            logger().check(m_dynamic_model != nullptr, "`dynamic_model` must not be null.");
            logger().check(duration >= Duration::zero(), "`duration` must be equal or greater than zero.");

            const Timestamp end_timestamp = start_timestamp + duration;

            check_resimulate_start_timestamp(start_timestamp, end_timestamp);

            clear_state_history_at_and_after(start_timestamp);
            simulate_until(end_timestamp);
            return current_state();
        }

        const StateEstimate<State>& resimulate_at_and_until(Timestamp start_timestamp, Timestamp end_timestamp) {
            LOG_METHOD();

            logger().check(m_dynamic_model != nullptr, "`dynamic_model` must not be null.");
            check_resimulate_start_timestamp(start_timestamp, end_timestamp);

            return resimulate_at_and_for(start_timestamp, end_timestamp - start_timestamp);
        }

        const StateEstimate<State>& resimulate_at_and_until_last_state(Timestamp start_timestamp) {
            LOG_METHOD();

            logger().check(m_dynamic_model != nullptr, "`dynamic_model` must not be null.");

            const Timestamp end_timestamp = current_state().timestamp;

            check_resimulate_start_timestamp(start_timestamp, end_timestamp);

            return resimulate_at_and_until(start_timestamp, end_timestamp);
        }

    private:
        std::unique_ptr<DynamicInterface<State>> m_dynamic_model;

        StateEstimate<State> m_initial_state;

        std::deque<StateEstimate<State>> m_state_history;
        std::map<Timestamp, EventBucket> m_event_history;

        Duration m_max_integration_step = std::chrono::milliseconds{10};
        Duration m_max_history_step = std::chrono::milliseconds{100};

    private:
        StateEstimate<State>& mutable_current_state() noexcept {
            return m_state_history.back();
        }

        void check_event_ptr(const EventPtr& event) {
            logger().check(event != nullptr, "`event` must not be null.");
            logger().check(
                event->timestamp() >= m_initial_state.timestamp,
                "An event timestamp must be at or after initial state timestamp."
            );
        }

        void insert_event_without_resimulation(EventPtr event) {
            LOG_METHOD();

            check_event_ptr(event);

            const Timestamp timestamp = event->timestamp();

            m_event_history[timestamp].push_back(std::move(event));
        }

        void reset_history_to_initial_state() {
            LOG_METHOD();

            m_state_history.clear();
            m_state_history.push_back(m_initial_state);

            apply_state_events(mutable_current_state());
        }

        void clear_state_history_at_and_after(Timestamp timestamp) {
            LOG_METHOD();

            if (timestamp <= m_initial_state.timestamp) {
                reset_history_to_initial_state();
                return;
            }

            while (m_state_history.back().timestamp >= timestamp) {
                m_state_history.pop_back();
            }
        }

        void apply_events_to_state(StateEstimate<State>& state, const EventBucket& events) {
            LOG_METHOD();

            logger().check(
                state.timestamp >= m_initial_state.timestamp,
                "A state cannot precede the initial state."
            );

            const Timestamp timestamp_before = state.timestamp;

            for (const EventPtr& event : events) {
                logger().check(event != nullptr, "A stored event must not be null.");

                logger().check(
                    event->timestamp() == timestamp_before,
                    "State and events must share the same timestamp."
                );

                event->apply(state);

                logger().check(
                    state.timestamp == timestamp_before,
                    "An event must not modify the state timestamp."
                );
            }
        }

        void apply_state_events(StateEstimate<State>& state) {
            LOG_METHOD();

            logger().check(state.timestamp >= m_initial_state.timestamp, "A state cannot precede the initial state.");

            auto it = m_event_history.find(state.timestamp);
            if (it != m_event_history.end() && !it->second.empty())
                apply_events_to_state(state, it->second);
        }

        const StateEstimate<State>& simulate_without_events_until(Timestamp end_timestamp) {
            LOG_METHOD();

            logger().check(m_dynamic_model != nullptr, "`dynamic_model` must not be null.");
            logger().check(end_timestamp >= current_state().timestamp, "`end_timestamp` cannot precede the current state.");

            StateEstimate<State> state = current_state();

            while (state.timestamp < end_timestamp) {
                const Timestamp next_timestamp = std::min(state.timestamp + m_max_history_step, end_timestamp);
                const Duration duration = next_timestamp - state.timestamp;

                logger().check(duration > Duration::zero(), "Simulation timestamp failed to advance.");

                m_dynamic_model->simulate_dynamic_inplace(
                    state.state, 
                    simulation::to_seconds(duration),
                    simulation::to_seconds(m_max_integration_step)
                );

                state.timestamp = next_timestamp;
                m_state_history.push_back(state);
            }

            return current_state();
        }

        void check_resimulate_start_timestamp(Timestamp start_timestamp, Timestamp end_timestamp) const {
            logger().check(start_timestamp >= m_initial_state.timestamp, "`start_timestamp` cannot precede the initial state.");
            logger().check(start_timestamp <= current_state().timestamp,"`start_timestamp` cannot be later than the current state.");
            logger().check(start_timestamp <= end_timestamp, "`start_timestamp` must not be later than `end_timestamp`.");
        }
    };
}
