#pragma once

#include <memory>
#include <utility>
#include <algorithm>
#include <vector>
#include <map>
#include <iterator>
#include <chrono>

#include "../../vulkan_self/logger/logger_header.h"
#include "dynamics/dynamic_interface.h"
#include "clock.h"
#include "state_estimate.h"
#include "events/instant_event.h"

namespace celeris {
    template<class State>
    class HybridDynamicalSystem {
    public:
        _XPARENT_NAME(HybridDynamicalSystem);

        using Timestamp = simulation::Timestamp;
        using Duration = simulation::Duration;

        using EventPtr = std::unique_ptr<InstantEvent<State>>;
        using EventBucket = std::vector<EventPtr>;

        using StateHistoryContainer = std::map<Timestamp, StateEstimate<State>>;
        using EventHistoryContainer = std::map<Timestamp, EventBucket>;

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

            m_state_history.insert({m_initial_state.timestamp, m_initial_state});
        }

        void insert_event(EventPtr event) {
            LOG_METHOD();

            check_event_ptr(event);

            const Timestamp timestamp = event->timestamp();
            m_event_history[timestamp].push_back(std::move(event));

            invalidate_cache_from(timestamp);
        }

        /*
            Эта функция небезопасна для использования! Если есть такая возможность,
            предпочтите использовать функцию define_state_at() вместо этой.

            Причина в том, что функция возвращает значение по ссылке, но ссылка
            ссылается на кеш, который в некоторых ситуациях может инвалидироваться
            (при вставке событий, timestamp которых равен или раньше текущего timestamp
            или очистке истории состояний)
        */
        [[nodiscard]]
        const StateEstimate<State>& define_state_at_ref_unsafe(Timestamp timestamp)
        {
            LOG_METHOD();

            logger().check(
                timestamp >= m_initial_state.timestamp,
                "`timestamp` cannot precede the initial state."
            );

            ensure_initial_state_cached();

            auto upper = m_state_history.lower_bound(timestamp);

            if (upper != m_state_history.end() && upper->first == timestamp) {
                return upper->second;
            }

            logger().check(
                upper != m_state_history.begin(),
                "A preceding state must exist."
            );

            const StateEstimate<State>* current = &std::prev(upper)->second;

            while (current->timestamp < timestamp) {
                const auto next_event = m_event_history.upper_bound(current->timestamp);

                const bool reaches_event =
                    next_event != m_event_history.end() &&
                    next_event->first <= timestamp;

                const Timestamp segment_end = reaches_event ? next_event->first : timestamp;
                
                StateEstimate<State>& next_state = simulate_between_events(segment_end, *current);

                if (reaches_event) {
                    apply_events_to_state(next_state, next_event->second);
                }

                current = &next_state;
            }

            return *current;
        }

        [[nodiscard]]
        StateEstimate<State> define_state_at(Timestamp timestamp) {
            LOG_METHOD();
            return define_state_at_ref_unsafe(timestamp);
        }

    private:
        std::unique_ptr<DynamicInterface<State>> m_dynamic_model;

        StateEstimate<State> m_initial_state;
        
        StateHistoryContainer m_state_history;
        EventHistoryContainer m_event_history;

        Duration m_max_integration_step = std::chrono::milliseconds{10};
        Duration m_max_history_step = std::chrono::milliseconds{100};

    private:
        StateEstimate<State>& simulate_between_events(
            Timestamp end_timestamp,
            const StateEstimate<State>& start_state)
        {
            logger().check(
                end_timestamp > start_state.timestamp,
                "`end_timestamp` must follow `start_state.timestamp`."
            );

            const auto event_it = m_event_history.upper_bound(start_state.timestamp);

            logger().check(
                event_it == m_event_history.end() ||
                event_it->first >= end_timestamp,
                "There must be no event strictly inside the integration interval."
            );

            StateEstimate<State> working_state = start_state;
            StateEstimate<State>* stored_state = nullptr;

            while (working_state.timestamp < end_timestamp) {
                const Duration remaining = end_timestamp - working_state.timestamp;
                const Duration duration = std::min(m_max_history_step, remaining);

                logger().check(
                    duration > Duration::zero(),
                    "Simulation timestamp failed to advance."
                );

                const Timestamp next_timestamp = working_state.timestamp + duration;

                m_dynamic_model->simulate_for_inplace(
                    working_state.timestamp,
                    working_state.state,
                    duration,
                    m_max_integration_step
                );

                working_state.timestamp = next_timestamp;

                auto [it, inserted] = m_state_history.emplace(next_timestamp, working_state);

                logger().check(
                    inserted,
                    "A state at this timestamp is already cached."
                );

                stored_state = &it->second;
            }

            return *stored_state;
        }

        void check_event_ptr(const EventPtr& event) {
            logger().check(event != nullptr, "`event` must not be null.");
            logger().check(
                event->timestamp() >= m_initial_state.timestamp,
                "An event timestamp must be at or after initial state timestamp."
            );
        }

        void invalidate_cache_from(Timestamp timestamp) {
            LOG_METHOD();

            m_state_history.erase(
                m_state_history.lower_bound(timestamp),
                m_state_history.end()
            );
        }

        void ensure_initial_state_cached() {
            LOG_METHOD();

            if (m_state_history.contains(m_initial_state.timestamp)) {
                return;
            }

            auto [it, inserted] = m_state_history.emplace(
                m_initial_state.timestamp,
                m_initial_state
            );

            logger().check(inserted, "The initial state could not be cached.");

            apply_state_events(it->second);
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
    };
}
