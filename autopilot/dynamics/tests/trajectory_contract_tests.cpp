#include <array>
#include <chrono>
#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "trajectory_contract_tests.h"

namespace {
    using Timestamp = celeris::simulation::Timestamp;
    using Value = celeris::simulation::Clock::rep;
    using Interface = celeris::TrajectoryInterface<Value>;
    using namespace std::chrono_literals;

    const std::array timestamps{
        Timestamp{5s},
        Timestamp{-1s},
        Timestamp{},
        Timestamp{1ns},
        Timestamp{15s}
    };

    static_assert(std::is_abstract_v<Interface>);
    static_assert(std::has_virtual_destructor_v<Interface>);
    static_assert(
        std::is_same_v<
            decltype(std::declval<Interface&>()(std::declval<Timestamp>())),
            Value>
    );

    void check(bool condition, const char* message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    class DirectTrajectory final : public Interface {
    private:
        Value evaluate_impl(Timestamp timestamp) override {
            return timestamp.time_since_epoch().count();
        }
    };

    class CachedTrajectory final : public Interface {
    public:
        std::size_t evaluation_count() const {
            return m_cache.size();
        }

    private:
        Value evaluate_impl(Timestamp timestamp) override {
            if (auto it = m_cache.find(timestamp); it != m_cache.end()) {
                return it->second;
            }

            const auto value = timestamp.time_since_epoch().count();
            m_cache.emplace(timestamp, value);
            return value;
        }

        std::map<Timestamp, Value> m_cache;
    };

    // У состояния нет operator==, оно передаётся только перемещением.
    struct MoveOnlyState {
        std::unique_ptr<Value> value;
    };

    class MoveOnlyTrajectory final : public celeris::TrajectoryInterface<MoveOnlyState> {
    private:
        MoveOnlyState evaluate_impl(Timestamp timestamp) override {
            return {std::make_unique<Value>(timestamp.time_since_epoch().count())};
        }
    };

    class CallCountTrajectory final : public Interface {
    private:
        Value evaluate_impl(Timestamp) override {
            return m_calls++;
        }

        Value m_calls = 0;
    };

    // Повторы одного времени стабильны, но первый запрос меняет всю функцию.
    class FirstQueryDependentTrajectory final : public Interface {
    private:
        Value evaluate_impl(Timestamp timestamp) override {
            if (!m_offset) {
                m_offset = timestamp.time_since_epoch().count();
            }

            return timestamp.time_since_epoch().count() + *m_offset;
        }

        std::optional<Value> m_offset;
    };

    void test_dynamic_dispatch_and_caching() {
        std::unique_ptr<Interface> direct = std::make_unique<DirectTrajectory>();
        auto cached = std::make_unique<CachedTrajectory>();
        Interface& cached_interface = *cached;

        for (auto timestamp : timestamps) {
            const auto expected = timestamp.time_since_epoch().count();
            check((*direct)(timestamp) == expected, "NVI lost the timestamp or result");
            check(cached_interface(timestamp) == expected, "Wrong cached result");
        }

        check(cached->evaluation_count() == timestamps.size(), "Cache was not populated");

        for (auto timestamp : timestamps) {
            check(cached_interface(timestamp) == (*direct)(timestamp), "Repeated result changed");
        }

        check(cached->evaluation_count() == timestamps.size(), "Cache grew on repeated queries");
    }

    void test_direct_trajectory() {
        celeris::tests::test_trajectory_contract<Value>(
            [] { return std::make_unique<DirectTrajectory>(); },
            timestamps
        );
    }

    void test_cached_trajectory() {
        celeris::tests::test_trajectory_contract<Value>(
            [] { return std::make_unique<CachedTrajectory>(); },
            timestamps
        );
    }

    void test_move_only_state() {
        celeris::tests::test_trajectory_contract<MoveOnlyState>(
            [] { return std::make_unique<MoveOnlyTrajectory>(); },
            timestamps,
            [](const MoveOnlyState& left, const MoveOnlyState& right) {
                return *left.value == *right.value;
            });
    }

    template<class Trajectory>
    void test_rejects_query_dependence() {
        try {
            celeris::tests::test_trajectory_contract<Value>(
                [] { return std::make_unique<Trajectory>(); },
                timestamps
            );
        } catch (const celeris::tests::TrajectoryContractViolation&) {
            return;
        }

        throw std::runtime_error("Contract test accepted a history-dependent trajectory");
    }
}

int main() {
    const std::array tests{
        std::pair{"dynamic dispatch and caching", &test_dynamic_dispatch_and_caching},
        std::pair{"direct trajectory", &test_direct_trajectory},
        std::pair{"cached trajectory", &test_cached_trajectory},
        std::pair{"move-only state without operator==", &test_move_only_state},
        std::pair{
            "rejects call-count dependence",
            &test_rejects_query_dependence<CallCountTrajectory>
        },
        std::pair{
            "rejects first-query dependence",
            &test_rejects_query_dependence<FirstQueryDependentTrajectory>
        }
    };

    int failures = 0;

    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "PASS: " << name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL: " << name << ": " << error.what() << '\n';
        } catch (...) {
            ++failures;
            std::cerr << "FAIL: " << name << ": unknown exception\n";
        }
    }

    return failures == 0 ? 0 : 1;
}
