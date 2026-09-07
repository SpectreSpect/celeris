#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>
#include <memory>
#include <optional>
#include <exception>

#include "../../vulkan_self/logger/logger_header.h"
#include "hybrid_dynamical_system.h"
#include "events/instant_event.h"
#include "state_estimate.h"

namespace celeris {
    template<class State>
    class HybridDynamicsRuntime {
    public:
        _XPARENT_NAME(HybridDynamicsRuntime);

        using EventPtr = std::unique_ptr<InstantEvent<State>>;
        using Timestamp = simulation::Timestamp;

        explicit HybridDynamicsRuntime(HybridDynamicalSystem<State> system)
            :   m_system(std::move(system)),
                m_latest_state_target(m_system.initial_state().timestamp),
                m_latest_state(m_system.initial_state()) {}
        
        HybridDynamicsRuntime(const HybridDynamicsRuntime&) = delete;
        HybridDynamicsRuntime& operator=(const HybridDynamicsRuntime&) = delete;

        HybridDynamicsRuntime(HybridDynamicsRuntime&&) = delete;
        HybridDynamicsRuntime& operator=(HybridDynamicsRuntime&&) = delete;

        ~HybridDynamicsRuntime() {
            stop();
        }
        
        void start() {
            LOG_METHOD();

            if (m_worker.joinable()) {
                return;
            }

            m_worker = std::jthread{
                [this](std::stop_token stop_token) {
                    worker_loop(stop_token);
                }
            };
        }

        void stop() {
            LOG_METHOD();

            if (!m_worker.joinable()) {
                return;
            }

            m_worker.request_stop();

            m_condition.notify_all();

            m_worker.join();
        }

        /*
            События накапливаются в очередь, но намеренно не пробуждают worker'а.
            Их обработка происходит при вызове request_update()
        */
        void submit_event(EventPtr event) {
            LOG_METHOD();

            logger().check(event != nullptr, "`event` must not be null.");

            std::lock_guard lock{m_input_mutex};
            m_pending_events.push_back(std::move(event));
        }

        void request_reset(StateEstimate<State> initial_state) {
            LOG_METHOD();

            {
                std::lock_guard lock{m_input_mutex};

                // Заменяется последним запросом
                m_requested_initial_state = std::move(initial_state);

                m_pending_events.clear();
                m_requested_timestamp.reset();
            }

            m_condition.notify_one();
        }

        void request_update(Timestamp target_timestamp) {
            {
                std::lock_guard lock{m_input_mutex};

                /*
                    Несколько запросов, пришедших пока поток занят,
                    объединяются в один последний запрос.
                */
                m_requested_timestamp = target_timestamp;
            }

            m_condition.notify_one();
        }

        
        // Возвращает результат последнего обработанного запроса
        [[nodiscard]]
        std::optional<StateEstimate<State>> latest_request_result() const {
            std::lock_guard lock{m_snapshot_mutex};
            return m_latest_request_result;
        }

        // Возвращает самое последнее состояние динамики на временной шкале
        [[nodiscard]]
        StateEstimate<State> latest_state() const {
            std::lock_guard lock{m_snapshot_mutex};
            return m_latest_state;
        }

    private:
        void worker_loop(std::stop_token stop_token) {
            LOG_METHOD();

            try {
                while (!stop_token.stop_requested()) {
                    std::vector<EventPtr> events;
                    
                    std::optional<StateEstimate<State>> reset_request;
                    std::optional<Timestamp> update_request;

                    {
                        std::unique_lock lock{m_input_mutex};

                        m_condition.wait(lock, [&](){
                            return stop_token.stop_requested() ||
                                m_requested_initial_state.has_value() ||
                                m_requested_timestamp.has_value();
                        });

                        if (stop_token.stop_requested()) {
                            return;
                        }

                        reset_request = std::move(m_requested_initial_state);
                        m_requested_initial_state.reset();
                        
                        update_request = std::move(m_requested_timestamp);
                        m_requested_timestamp.reset();

                        if (update_request.has_value()) {
                            events.swap(m_pending_events);
                        }
                    }

                    if (reset_request.has_value()) {
                        m_system.reset(std::move(*reset_request));
                        m_latest_state_target = m_system.initial_state().timestamp;
                    }

                    const Timestamp minimal_timestamp = m_system.initial_state().timestamp;

                    if (update_request.has_value()) {
                        
                        // Здесь позже можно будет вернуть вставку событий батчами #TODO
                        for (EventPtr& event : events) {
                            if (event->timestamp() < minimal_timestamp) 
                                continue;
                            
                            m_system.insert_event(std::move(event));
                        }

                        const bool update_out_of_bounds = *update_request < minimal_timestamp;

                        const Timestamp effective_timestamp = 
                            update_out_of_bounds ? minimal_timestamp : *update_request;
                        
                        if (effective_timestamp > m_latest_state_target) {
                            m_latest_state_target = effective_timestamp;
                        }
    
                        if (update_out_of_bounds) {
                            logger().log(
                                clr(
                                    "Requested timestamp precedes the initial state; "
                                    "the initial timestamp will be used instead.",
                                    LoggerPalette::warning
                                )
                            );
                        }

                        StateEstimate<State> latest_request_state = m_system.define_state_at(effective_timestamp);
                        StateEstimate<State> latest_state = m_system.define_state_at(m_latest_state_target);

                        {
                            std::lock_guard lock{m_snapshot_mutex};
                            m_latest_request_result = std::move(latest_request_state);
                            m_latest_state = std::move(latest_state);
                        }
                    } else if (reset_request.has_value()) {
                        std::lock_guard lock{m_snapshot_mutex};

                         m_latest_state = m_system.initial_state();
                         m_latest_request_result = m_latest_state;
                    }
                }
            } catch (const std::exception& exception) {
                logger().log_traceback();
                logger().log_error(exception.what());
            } catch (...) {
                logger().log_traceback();
                logger().log_error("Unknown exception in HybridDynamicsRuntime worker.");
            }
        }

    private:
        HybridDynamicalSystem<State> m_system;

        std::mutex m_input_mutex;
        std::condition_variable m_condition;
        std::vector<EventPtr> m_pending_events;
        std::optional<Timestamp> m_requested_timestamp;
        std::optional<StateEstimate<State>> m_requested_initial_state;

        mutable std::mutex m_snapshot_mutex;
        std::optional<StateEstimate<State>> m_latest_request_result; // Результат последнего запроса состояния

        Timestamp m_latest_state_target;
        StateEstimate<State> m_latest_state; // самое последнее состояние на временной шкале

        std::jthread m_worker;
    };
}
