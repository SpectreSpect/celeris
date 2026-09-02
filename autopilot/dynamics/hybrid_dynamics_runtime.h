#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>
#include <memory>
#include <optional>

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
            :   m_system(std::move(system)) {}
        
        HybridDynamicsRuntime(const HybridDynamicsRuntime&) = delete;
        HybridDynamicsRuntime& operator=(const HybridDynamicsRuntime&) = delete;

        HybridDynamicsRuntime(HybridDynamicsRuntime&&) noexcept = delete;
        HybridDynamicsRuntime& operator=(HybridDynamicsRuntime&&) noexcept = delete;

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

        void submit_event(EventPtr event) {
            LOG_METHOD();

            logger().check(event != nullptr, "`event` must not be null.");

            std::lock_guard lock{m_input_mutex};
            m_pending_events.push_back(std::move(event));
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

        [[nodiscard]]
        std::optional<StateEstimate<State>> latest_state() const {
            std::lock_guard lock{m_snapshot_mutex};
            return m_latest_snapshot;
        }

    private:
        void worker_loop(std::stop_token stop_token) {
            LOG_METHOD();

            try {
                while (!stop_token.stop_requested()) {
                    std::vector<EventPtr> events;
                    Timestamp target_timestamp;

                    {
                        std::unique_lock lock{m_input_mutex};

                        m_condition.wait(lock, [&](){
                            return stop_token.stop_requested() ||
                                m_requested_timestamp.has_value();
                        });

                        if (stop_token.stop_requested()) {
                            return;
                        }

                        events.swap(m_pending_events);

                        target_timestamp = *m_requested_timestamp;
                        m_requested_timestamp.reset();
                    }

                    // Здесь позже можно будет вернуть вставку событий батчами #TODO
                    for (EventPtr& event : events) {
                        m_system.insert_event(std::move(event));
                    }

                    StateEstimate<State> state = m_system.define_state_at(target_timestamp);

                    {
                        std::lock_guard lock{m_snapshot_mutex};
                        m_latest_snapshot = std::move(state);
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

        mutable std::mutex m_snapshot_mutex;
        std::optional<StateEstimate<State>> m_latest_snapshot;

        std::jthread m_worker;
    };
}
