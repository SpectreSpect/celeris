#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "../trajectory_adapter.h"
#include "component_test_models.h"
#include "evaluator_test_models.h"
#include "prescribed_values_contract_tests.h"
#include "trajectory_cache_contract_tests.h"
#include "trajectory_evaluator_contract_tests.h"

namespace {
    using namespace celeris;
    using namespace celeris::tests;
    using namespace celeris::tests::models;
    using namespace std::chrono_literals;
    using Timestamp = simulation::Timestamp;

    const std::array<std::pair<Timestamp, int>, 4> samples{{
        {Timestamp{10ns}, 10},
        {Timestamp{-5ns}, 20},
        {Timestamp{}, 30},
        {Timestamp{20ns}, 40}
    }};

    struct IncompleteCache {
        std::optional<int> find(Timestamp) const;

        void store(Timestamp, int);
    };

    struct CopyState {
        explicit CopyState(int input)
            :   value(input) {}

        CopyState(const CopyState&) = default;
        CopyState(CopyState&&) = default;
        CopyState& operator=(const CopyState&) = delete;
        CopyState& operator=(CopyState&&) = delete;

        int value;
    };

    class ContractOnlyCache final : public MapCache<CopyState> {
    public:
        using MapCache<CopyState>::find;
        using MapCache<CopyState>::invalidate;

        ContractOnlyCache() = default;
        ContractOnlyCache(const ContractOnlyCache&) = delete;

        std::optional<CopyState> find(Timestamp) = delete;

        void store(Timestamp time, CopyState&& value) {
            MapCache<CopyState>::store(time, std::move(value));
        }

        void invalidate(const IntervalInvalidationRegion&) = delete;
    };

    class ContractOnlySource final : public NoArrowSource<CopyState> {
    public:
        explicit ContractOnlySource(Timestamp origin) {
            NoArrowSource<CopyState>::set(origin, CopyState{10});
        }

        ContractOnlySource(const ContractOnlySource&) = delete;

        void set(Timestamp& time, CopyState&& value) {
            NoArrowSource<CopyState>::set(time, std::move(value));
        }

        void erase(Timestamp& time) {
            NoArrowSource<CopyState>::erase(time);
        }
    };

    class CopyStateEvaluator final : public TrajectoryEvaluatorInterface<CopyState> {
    private:
        CopyState evaluate_impl(Timestamp, TrajectoryEvaluationContext<CopyState>&) override {
            return CopyState{200};
        }

        std::unique_ptr<InvalidationRegionInterface> invalidation_region_impl(
            const PrescribedValuesViewInterface<CopyState>&,
            const PrescribedValuesChange<CopyState>&) const override
        {
            return all_times();
        }
    };

    static_assert(trajectory_cache<MapCache<int>, int>);
    static_assert(trajectory_cache<NoCache<int>, int>);
    static_assert(!trajectory_cache<IncompleteCache, int>);
    static_assert(prescribed_values_source<MapSource<int>, int>);
    static_assert(prescribed_values_source<VectorSource<int>, int>);
    static_assert(prescribed_values_source<NoArrowSource<int>, int>);
    static_assert(!prescribed_values_source<std::map<Timestamp, int>, int>);
    static_assert(trajectory_cache<MapCache<CopyState>, CopyState>);
    static_assert(prescribed_values_source<VectorSource<CopyState>, CopyState>);
    static_assert(trajectory_cache<ContractOnlyCache, CopyState>);
    static_assert(prescribed_values_source<ContractOnlySource, CopyState>);
    static_assert(!std::default_initializable<CopyState>);
    static_assert(!std::is_copy_assignable_v<CopyState>);

    template<template<class> class Cache>
    void check_cache() {
        test_trajectory_cache_contract<int>([] { return Cache<int>{}; }, samples);
    }

    template<template<class> class Source>
    void check_source() {
        test_prescribed_values_contract<int>([] { return Source<int>{}; }, samples);
    }

    void check_copy_state() {
        const std::array<std::pair<Timestamp, CopyState>, 3> states{{
            {Timestamp{2ns}, CopyState{10}},
            {Timestamp{}, CopyState{20}},
            {Timestamp{1ns}, CopyState{30}}
        }};

        auto equivalent = [](const CopyState& left, const CopyState& right) {
            return left.value == right.value;
        };

        test_trajectory_cache_contract<CopyState>(
            [] { return MapCache<CopyState>{}; },
            states,
            equivalent
        );
        test_prescribed_values_contract<CopyState>(
            [] { return MapSource<CopyState>{}; },
            states,
            equivalent
        );
        test_prescribed_values_contract<CopyState>(
            [] { return VectorSource<CopyState>{}; },
            states,
            equivalent
        );
    }

    void check_adapter_with_minimal_components() {
        const Timestamp origin{};
        const Timestamp time{1s};
        auto cache = std::make_unique<ContractOnlyCache>();
        cache->store(time, CopyState{99});

        TrajectoryAdapter<CopyState, ContractOnlyCache, ContractOnlySource> adapter(
            std::move(cache),
            std::make_unique<ContractOnlySource>(origin),
            std::make_unique<CopyStateEvaluator>()
        );
        TrajectoryInterface<CopyState>& trajectory = adapter;

        require(trajectory(time).value == 200, "Adapter reused an initial cache entry");
        require(trajectory(origin).value == 10, "Adapter ignored an initial prescribed value");

        adapter.change(SetPrescribedValue<CopyState>{time, CopyState{42}});
        require(trajectory(time).value == 42, "Adapter did not commit a prescribed value");

        adapter.change(ErasePrescribedValue{time});
        require(trajectory(time).value == 200, "Adapter did not erase a prescribed value");
    }

    template<invalidation_region Region>
    void check_region(const Region& region, std::span<const Timestamp> probes) {
        for (auto time : probes) {
            const bool actual = region(time);
            require(actual == region(time), "Region membership is unstable");

            if (auto intervals = invalidation_intervals(region)) {
                const bool expected = std::any_of(
                    intervals->begin(),
                    intervals->end(),
                    [&](const auto& interval) { return interval.contains(time); });
                require(actual == expected, "Intervals disagree with region membership");
            }
        }
    }

    struct StaticIntervals {
        std::array<TimeInterval, 1> data{{{Timestamp{}, Timestamp{10ns}}}};

        bool operator()(Timestamp time) const {
            return data[0].contains(time);
        }

        std::span<const TimeInterval> intervals() const {
            return data;
        }
    };

    class DishonestIntervals final : public IntervalInvalidationRegionInterface {
    public:
        std::span<const TimeInterval> intervals() const override {
            return m_data;
        }

    private:
        bool contains_impl(Timestamp) const override {
            return false;
        }

        std::array<TimeInterval, 1> m_data{{TimeInterval{}}};
    };

    void check_regions_and_boundaries() {
        const std::array probes{
            Timestamp::min(),
            Timestamp{-4ns},
            Timestamp{-3ns},
            Timestamp{-1ns},
            Timestamp{},
            Timestamp{1ns},
            Timestamp{2ns},
            Timestamp{4ns},
            Timestamp{5ns},
            Timestamp::max()
        };
        IntervalInvalidationRegion region({
            {Timestamp{4ns}, std::nullopt},
            {Timestamp{-3ns}, Timestamp{}},
            {Timestamp{2ns}, Timestamp{5ns}}
        });
        const InvalidationRegionInterface& erased = region;

        check_region(erased, probes);
        require(
            !erased(Timestamp::min()) && erased(Timestamp::max()),
            "Unbounded interval is wrong"
        );
        require(erased(Timestamp{-3ns}) && !erased(Timestamp{}), "Half-open boundary is wrong");
        require(erased(Timestamp{2ns}) && !erased(Timestamp{1ns}), "Union has a wrong gap");

        IntervalInvalidationRegion empty({});
        IntervalInvalidationRegion all({TimeInterval{}});
        IntervalInvalidationRegion last({{Timestamp::max(), std::nullopt}});
        IntervalInvalidationRegion reversed({{Timestamp{5ns}, Timestamp{2ns}}});
        check_region(empty, probes);
        check_region(all, probes);
        check_region(last, probes);
        check_region(reversed, probes);

        for (auto time : probes) {
            require(!empty(time) && all(time) && !reversed(time), "Empty/all interval is wrong");
            require(last(time) == (time == Timestamp::max()), "Last Timestamp was lost");
        }

        StaticIntervals statically_known;
        static_assert(interval_invalidation_region<StaticIntervals>);
        check_region(statically_known, probes);
        require(invalidation_intervals(statically_known).has_value(), "Static capability was lost");

        auto plain = [](Timestamp time) { return time == Timestamp{}; };
        require(!invalidation_intervals(plain), "Plain predicate falsely advertises intervals");
    }

    void check_interval_optimization() {
        MapCache<int> cache;

        for (const auto& [time, value] : samples) {
            cache.store(time, value);
        }

        StaticIntervals statically_known;
        cache.invalidate(statically_known);
        require(
            cache.interval_operations == 1 && cache.predicate_checks == 0,
            "Static interval path scanned predicates"
        );
        require(
            !cache.find(Timestamp{}) && cache.find(Timestamp{10ns}).has_value(),
            "Static interval path mishandled endpoints"
        );

        IntervalInvalidationRegion dynamic({{Timestamp{10ns}, std::nullopt}});
        const InvalidationRegionInterface& erased = dynamic;
        cache.invalidate(erased);
        require(
            cache.interval_operations == 2 && cache.predicate_checks == 0,
            "Dynamic interval capability was lost"
        );
        require(
            !cache.find(Timestamp{10ns}) && cache.find(Timestamp{-5ns}).has_value(),
            "Dynamic interval path deleted the wrong range"
        );
    }

    void check_cache_after_predicate_exception() {
        MapCache<int> cache;

        for (const auto& [time, value] : samples) {
            cache.store(time, value);
        }

        int calls = 0;
        PredicateInvalidationRegion throwing(
            [&](Timestamp) {
                if (++calls == 2) {
                    throw InjectedFailure();
                }

                return true;
            });
        expect_exception<InjectedFailure>(
            [&] { cache.invalidate(throwing); },
            "Predicate was not invoked"
        );

        for (const auto& [time, value] : samples) {
            auto remaining = cache.find(time);
            require(!remaining || *remaining == value, "Interrupted invalidation damaged an entry");
        }

        cache.store(Timestamp{}, 123);
        require(cache.find(Timestamp{}) == 123, "Cache is unusable after predicate failure");
    }

    struct ResetValueFailure {
        ~ResetValueFailure() {
            ThrowingValue::copies_before_failure = -1;
        }
    };

    /*
        Внедрение ошибки в каждую последовательную точку копирования. Нормальные
        generic-тесты не способны сами вызвать исключение произвольной модели.
    */
    template<template<class> class Source>
    void check_source_exception_guarantee() {
        ResetValueFailure cleanup;

        for (int operation = 0; operation < 3; ++operation) {
            bool reached_success = false;

            for (int budget = 0; budget < 20 && !reached_success; ++budget) {
                ThrowingValue::copies_before_failure = -1;
                Source<ThrowingValue> source;
                source.set(Timestamp{}, ThrowingValue{1});
                source.set(Timestamp{10ns}, ThrowingValue{2});
                const std::map<Timestamp, int> before{{Timestamp{}, 1}, {Timestamp{10ns}, 2}};
                ThrowingValue replacement{9};
                ThrowingValue::copies_before_failure = budget;
                bool failed = false;

                try {
                    if (operation == 0) {
                        source.set(Timestamp{}, replacement);
                    } else if (operation == 1) {
                        source.set(Timestamp{5ns}, replacement);
                    } else {
                        source.erase(Timestamp{});
                    }
                } catch (const InjectedFailure&) {
                    failed = true;
                }

                ThrowingValue::copies_before_failure = -1;
                std::map<Timestamp, int> actual;

                for (const auto& [time, value] : source) {
                    actual.emplace(time, value.value);
                }

                if (failed) {
                    require(
                        actual == before,
                        "Failed source mutation changed its logical contents"
                    );
                } else {
                    auto expected = before;

                    if (operation == 2) {
                        expected.erase(Timestamp{});
                    } else {
                        expected.insert_or_assign(operation == 0 ? Timestamp{} : Timestamp{5ns}, 9);
                    }

                    require(actual == expected, "Successful source mutation has wrong contents");
                    reached_success = true;
                }
            }

            require(reached_success, "Failure fixture never reached a successful operation");
        }
    }

    void reject_stale_cache() {
        expect_exception<ContractViolation>(
            check_cache<StaleCache>,
            "Suite accepted stale cache values"
        );
    }

    void reject_uninvalidated_cache() {
        expect_exception<ContractViolation>(
            check_cache<UninvalidatedCache>,
            "Suite accepted missing invalidation"
        );
    }

    void reject_wrong_bounds() {
        expect_exception<ContractViolation>(
            check_source<WrongUpperBoundSource>,
            "Suite accepted wrong source bounds"
        );
    }

    void reject_weak_source_exception() {
        expect_exception<ContractViolation>(
            check_source_exception_guarantee<WeakExceptionSource>,
            "Suite accepted a partial source write after failure"
        );
    }

    void reject_dishonest_intervals() {
        const std::array probes{Timestamp{}, Timestamp::min(), Timestamp::max()};
        DishonestIntervals region;
        expect_exception<ContractViolation>(
            [&] { check_region(region, probes); },
            "Suite accepted inconsistent interval capability"
        );
    }

    const std::array evaluator_times{
        Timestamp{2s},
        Timestamp{-1ns},
        Timestamp{},
        Timestamp{1ns},
        Timestamp{1s},
        Timestamp{3s},
        Timestamp::min(),
        Timestamp::max()
    };
    const std::array<PrescribedValuesChange<int>, 8> changes{{
        SetPrescribedValue<int>{Timestamp{}, 20},
        ErasePrescribedValue{Timestamp{}},
        SetPrescribedValue<int>{Timestamp{1s}, 40},
        ErasePrescribedValue{Timestamp{1s}},
        SetPrescribedValue<int>{Timestamp{3s}, 70},
        ErasePrescribedValue{Timestamp{3s}},
        SetPrescribedValue<int>{Timestamp::max(), 90},
        ErasePrescribedValue{Timestamp::max()}
    }};

    template<class Evaluator, template<class> class Source = MapSource>
    auto make_evaluator_fixture(bool seed = true) {
        auto source = std::make_unique<Source<int>>();

        if (seed) {
            source->set(Timestamp{}, 10);
            source->set(Timestamp{3s}, 55);
        }

        return std::make_unique<EvaluatorContractFixture<int, Source<int>>>(
            std::move(source),
            std::make_unique<Evaluator>()
        );
    }

    template<class Evaluator, template<class> class Source = MapSource>
    void check_evaluator() {
        test_trajectory_evaluator_contract<int>(
            [] { return make_evaluator_fixture<Evaluator, Source>(); },
            evaluator_times,
            changes
        );
    }

    void check_empty_source_evaluator() {
        test_trajectory_evaluator_contract<int>(
            [] { return make_evaluator_fixture<StepEvaluator>(false); },
            evaluator_times,
            changes
        );
    }

    void check_evaluator_replacement() {
        test_evaluator_replacement_contract<int>(
            [] { return make_evaluator_fixture<StepEvaluator>(); },
            [] { return make_evaluator_fixture<FlatEvaluator>(); },
            [] { return std::make_unique<FlatEvaluator>(); },
            evaluator_times
        );
        test_evaluator_replacement_contract<int>(
            [] { return make_evaluator_fixture<FlatEvaluator>(); },
            [] { return make_evaluator_fixture<StepEvaluator>(); },
            [] { return std::make_unique<StepEvaluator>(); },
            evaluator_times
        );
    }

    class FailingInvalidationCache final : public MapCache<int> {
    public:
        void invalidate(const InvalidationRegionInterface& region) {
            MapCache<int>::invalidate(region);

            if (++m_invalidations == 2) {
                throw InjectedFailure();
            }
        }

    private:
        int m_invalidations = 0;
    };

    void check_evaluator_replacement_after_failure() {
        TrajectoryAdapter<int, FailingInvalidationCache, MapSource<int>> trajectory(
            std::make_unique<FailingInvalidationCache>(),
            std::make_unique<MapSource<int>>(),
            std::make_unique<FlatEvaluator>(10)
        );
        const Timestamp time{};
        require(trajectory(time) == 10, "Unexpected value before replacement");

        expect_exception<InjectedFailure>(
            [&] { trajectory.replace_evaluator(std::make_unique<FlatEvaluator>(20)); },
            "Expected failure while invalidating the cache for replacement"
        );
        require(trajectory(time) == 10, "Failed replacement discarded the old evaluator");

        trajectory.replace_evaluator(std::make_unique<FlatEvaluator>(20));
        require(trajectory(time) == 20, "Evaluator replacement did not recover");
    }

    void require_same_source_reads(
        const PrescribedValuesViewInterface<int>& actual,
        const PrescribedValuesViewInterface<int>& expected)
    {
        for (auto time : evaluator_times) {
            require(actual.find(time) == expected.find(time), "Wrong exact source lookup");
            require(actual.lower_bound(time) == expected.lower_bound(time), "Wrong lower bound");
            require(actual.upper_bound(time) == expected.upper_bound(time), "Wrong upper bound");
            require(
                actual.at_or_before(time) == expected.at_or_before(time),
                "Wrong preceding entry"
            );
        }
    }

    class ExpectedSourceEvaluator final : public FlatEvaluator {
    public:
        explicit ExpectedSourceEvaluator(const PrescribedValuesViewInterface<int>& expected)
            :   m_expected(expected) {}

    private:
        bool compatible_impl(const PrescribedValuesViewInterface<int>& values) const override {
            require_same_source_reads(values, m_expected);
            return true;
        }

        const PrescribedValuesViewInterface<int>& m_expected;
    };

    void check_prospective_source_reads() {
        MapSource<int> expected;
        const PrescribedValuesView<int, MapSource<int>> expected_view(expected);
        TrajectoryAdapter<int, MapCache<int>, NoArrowSource<int>> trajectory(
            std::make_unique<MapCache<int>>(),
            std::make_unique<NoArrowSource<int>>(),
            std::make_unique<ExpectedSourceEvaluator>(expected_view)
        );

        const std::array<PrescribedValuesChange<int>, 8> edits{{
            SetPrescribedValue<int>{Timestamp::min(), 10},
            SetPrescribedValue<int>{Timestamp::max(), 20},
            SetPrescribedValue<int>{Timestamp{}, 30},
            SetPrescribedValue<int>{Timestamp{}, 40},
            ErasePrescribedValue{Timestamp{1ns}},
            ErasePrescribedValue{Timestamp{}},
            ErasePrescribedValue{Timestamp::min()},
            ErasePrescribedValue{Timestamp::max()}
        }};

        for (const auto& edit : edits) {
            apply_prescribed_values_change<int>(expected, edit);
            trajectory.change(edit);
            require_same_source_reads(trajectory.source_view(), expected_view);
        }
    }

    void check_explicit_compatibility() {
        auto live = make_evaluator_fixture<StepEvaluator>(false);
        const int before = (*live)(Timestamp{2s});

        expect_exception<std::invalid_argument>(
            [&] {
                live->replace_evaluator(std::make_unique<RequiresOriginEvaluator>());
            },
            "Incompatible replacement was accepted"
        );
        require((*live)(Timestamp{2s}) == before, "Rejected replacement changed trajectory");
        expect_exception<std::invalid_argument>(
            [] {
                (void)make_evaluator_fixture<RequiresOriginEvaluator>(false);
            },
            "Incompatible initial source was accepted"
        );

        auto requires_origin = make_evaluator_fixture<RequiresOriginEvaluator>();
        const int old = (*requires_origin)(Timestamp{2s});

        expect_exception<std::invalid_argument>(
            [&] {
                requires_origin->change(ErasePrescribedValue{Timestamp{}});
            },
            "Incompatible source change was accepted"
        );
        require(
            requires_origin->source_view().find(Timestamp{}) == 10,
            "Rejected change removed a required source value"
        );
        require((*requires_origin)(Timestamp{2s}) == old, "Rejected change affected trajectory");
    }

    void check_evaluation_exception_recovery() {
        auto live = make_evaluator_fixture<ThrowOnceEvaluator>();

        expect_exception<InjectedFailure>(
            [&] { (void)(*live)(Timestamp{2s}); },
            "Expected evaluation failure"
        );
        require(!live->cached(Timestamp{2s}), "Partial failed result entered cache");
        require(live->cached(Timestamp{1s}) == 12, "Completed child result was corrupted");
        require((*live)(Timestamp{2s}) == 14, "Evaluation stack was not restored after failure");
    }

    void check_update_exception_recovery() {
        auto fail = std::make_shared<bool>(false);
        auto source = std::make_unique<FailCommitSource>(fail);
        source->set(Timestamp{}, 10);

        EvaluatorContractFixture<int, FailCommitSource> live(
            std::move(source),
            std::make_unique<StepEvaluator>()
        );
        require(live(Timestamp{2s}) == 14, "Wrong pre-update value");

        *fail = true;
        const PrescribedValuesChange<int> change = SetPrescribedValue<int>{Timestamp{}, 80};

        expect_exception<InjectedFailure>(
            [&] { live.change(change); },
            "Expected source commit failure"
        );
        require(live.source_view().find(Timestamp{}) == 10, "Failed commit changed source");
        require(
            !live.cached(Timestamp{2s}),
            "Failure fixture did not reach invalidation before commit"
        );
        require(live(Timestamp{2s}) == 14, "Failed commit left a stale trajectory");

        live.change(change);
        require(live(Timestamp{2s}) == 84, "Source update did not recover after failure");
    }

    void check_region_exception_recovery() {
        auto live = make_evaluator_fixture<ThrowingRegionEvaluator>();
        require((*live)(Timestamp{2s}) == 14, "Wrong pre-update value");
        (void)(*live)(Timestamp{-1ns});
        const PrescribedValuesChange<int> change = SetPrescribedValue<int>{Timestamp{}, 80};

        expect_exception<InjectedFailure>(
            [&] { live->change(change); },
            "Expected invalidation failure"
        );
        require(live->source_view().find(Timestamp{}) == 10, "Invalidation failure changed source");
        require((*live)(Timestamp{2s}) == 14, "Interrupted invalidation damaged trajectory");

        live->change(change);
        require((*live)(Timestamp{2s}) == 84, "Update did not recover after predicate failure");
    }

    void reject_wrong_evaluator_invalidation() {
        expect_exception<ContractViolation>(
            check_evaluator<WrongInvalidationEvaluator>,
            "Suite accepted an incomplete invalidation region"
        );
    }

    void reject_history_dependent_evaluator() {
        expect_exception<ContractViolation>(
            check_evaluator<FirstQueryEvaluator>,
            "Suite accepted query-history-dependent evaluation"
        );
    }

    void reject_wrong_ranks() {
        auto live = make_evaluator_fixture<WrongRankEvaluator>(false);
        (void)(*live)(Timestamp{});
        require(live->cached(Timestamp{}) == 10, "Rank test requires a cached dependency");

        expect_exception<std::logic_error>(
            [&] { (void)(*live)(Timestamp{1s}); },
            "Rank check accepted a non-decreasing cached dependency"
        );
    }

    void reject_dependency_cycle() {
        auto live = make_evaluator_fixture<CyclicEvaluator>(false);
        expect_exception<std::logic_error>(
            [&] { (void)(*live)(Timestamp{}); },
            "Cycle detection accepted recursive self-dependency"
        );
    }

    void reject_null_region() {
        auto live = make_evaluator_fixture<NullRegionEvaluator>();

        expect_exception<std::logic_error>(
            [&] { live->change(changes[0]); },
            "NVI accepted a null invalidation region"
        );
        require(live->source_view().find(Timestamp{}) == 10, "Null region changed source");
    }
}

int main() {
    const std::array tests{
        std::pair{"map cache contract", &check_cache<MapCache>},
        std::pair{"single-entry eviction cache contract", &check_cache<SingleEntryCache>},
        std::pair{"no-storage cache contract", &check_cache<NoCache>},
        std::pair{"map prescribed values contract", &check_source<MapSource>},
        std::pair{"vector prescribed values contract", &check_source<VectorSource>},
        std::pair{"source iterator without operator->", &check_source<NoArrowSource>},
        std::pair{
            "copy construction without assignment, default constructor or equality",
            &check_copy_state
        },
        std::pair{"region boundaries and static capability", &check_regions_and_boundaries},
        std::pair{"static and dynamic interval optimization", &check_interval_optimization},
        std::pair{
            "cache remains usable after predicate failure",
            &check_cache_after_predicate_exception
        },
        std::pair{
            "map source strong exception guarantee",
            &check_source_exception_guarantee<MapSource>
        },
        std::pair{
            "vector source strong exception guarantee",
            &check_source_exception_guarantee<VectorSource>
        },
        std::pair{"rejects stale cache", &reject_stale_cache},
        std::pair{"rejects missing invalidation", &reject_uninvalidated_cache},
        std::pair{"rejects wrong source bounds", &reject_wrong_bounds},
        std::pair{"rejects weak source exception guarantee", &reject_weak_source_exception},
        std::pair{"rejects dishonest interval capability", &reject_dishonest_intervals},
        std::pair{"ranked recursive evaluator contract", &check_evaluator<StepEvaluator>},
        std::pair{"unranked recursive evaluator contract", &check_evaluator<PlainStepEvaluator>},
        std::pair{
            "dynamic evaluator over vector source",
            &check_evaluator<StepEvaluator, VectorSource>
        },
        std::pair{"source-free evaluator initial state", &check_empty_source_evaluator},
        std::pair{
            "constant evaluator with conservative invalidation",
            &check_evaluator<FlatEvaluator>
        },
        std::pair{"invalidation borrowing old source", &check_evaluator<BorrowingRegionEvaluator>},
        std::pair{"replace evaluator with warmed cache", &check_evaluator_replacement},
        std::pair{
            "retry evaluator replacement after failure",
            &check_evaluator_replacement_after_failure
        },
        std::pair{
            "read proposed source during compatibility check",
            &check_prospective_source_reads
        },
        std::pair{
            "adapter accepts minimal component operations",
            &check_adapter_with_minimal_components
        },
        std::pair{"explicit source/evaluator compatibility", &check_explicit_compatibility},
        std::pair{
            "evaluation exception and recursion recovery",
            &check_evaluation_exception_recovery
        },
        std::pair{"source commit failure after invalidation", &check_update_exception_recovery},
        std::pair{"invalidation failure leaves old source valid", &check_region_exception_recovery},
        std::pair{
            "rejects incomplete evaluator invalidation",
            &reject_wrong_evaluator_invalidation
        },
        std::pair{
            "rejects evaluator query-history dependence",
            &reject_history_dependent_evaluator
        },
        std::pair{"rejects non-decreasing ranks", &reject_wrong_ranks},
        std::pair{"rejects recursive dependency cycle", &reject_dependency_cycle},
        std::pair{"rejects null invalidation region", &reject_null_region}
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
