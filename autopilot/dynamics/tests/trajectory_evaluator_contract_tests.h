#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "../trajectory_adapter.h"
#include "component_contract_test_support.h"
#include "component_test_models.h"
#include "trajectory_contract_tests.h"

namespace celeris::tests {
    /*
        Фикстура запускает TrajectoryAdapter и наблюдает его кэш и вычислитель
        через ссылки для чтения. Все запросы и изменения выполняет сам адаптер.
    */
    template<class Y, prescribed_values_source<Y> Source>
    class EvaluatorContractFixture {
        using Cache = models::MapCache<Y>;

    public:
        using Timestamp = simulation::Timestamp;
        using Evaluator = TrajectoryEvaluatorInterface<Y>;

        EvaluatorContractFixture(
            std::unique_ptr<Source> source,
            std::unique_ptr<Evaluator> evaluator)
            :   EvaluatorContractFixture(
                    std::make_unique<Cache>(),
                    std::move(source),
                    std::move(evaluator)
                ) {}

        Y operator()(Timestamp time) {
            return m_adapter(time);
        }

        void change(const PrescribedValuesChange<Y>& change) {
            m_adapter.change(change);
        }

        void replace_evaluator(std::unique_ptr<Evaluator> evaluator) {
            const auto* replacement = evaluator.get();
            m_adapter.replace_evaluator(std::move(evaluator));
            m_evaluator = replacement;
        }

        const PrescribedValuesViewInterface<Y>& source_view() const {
            return m_adapter.source_view();
        }

        const Evaluator& evaluator() const {
            return *m_evaluator;
        }

        std::optional<Y> cached(Timestamp time) const {
            return m_cache->find(time);
        }

    private:
        EvaluatorContractFixture(
            std::unique_ptr<Cache> cache,
            std::unique_ptr<Source> source,
            std::unique_ptr<Evaluator> evaluator)
            :   m_cache(cache.get()),
                m_evaluator(evaluator.get()),
                m_adapter(std::move(cache), std::move(source), std::move(evaluator)) {}

        const Cache* m_cache;
        const Evaluator* m_evaluator;
        TrajectoryAdapter<Y, Cache, Source> m_adapter;
    };

    /*
        Factory создаёт свежую независимую fixture с одинаковыми E и D.
        Изменения совместимы последовательно; timestamps содержит >=2 разных T.
        После каждого изменения проверяется как граница самого предиката,
        так и результат при прогретом кэше. Эталоны вычисляются на холодных
        экземплярах с теми же исходными данными. Все времена могут запрашиваться.
        Завершение произвольного кода этими конечными примерами не доказывается.
    */
    template<class Y, class Factory, class Equivalent = std::equal_to<>>
    void test_trajectory_evaluator_contract(
        Factory make_fixture,
        std::span<const simulation::Timestamp> timestamps,
        std::span<const PrescribedValuesChange<Y>> changes,
        Equivalent equivalent = {})
    {
        try {
            test_trajectory_contract<Y>(make_fixture, timestamps, equivalent);
        } catch (const TrajectoryContractViolation& error) {
            throw ContractViolation(error.what());
        }

        auto rank_fixture = make_fixture();

        if (const auto* ranked = dynamic_cast<const RankedTrajectoryEvaluatorInterface<Y>*>(
            &rank_fixture->evaluator())) {
            std::vector<typename RankedTrajectoryEvaluatorInterface<Y>::Rank> expected;

            for (auto time : timestamps) {
                expected.push_back(ranked->rank(time, rank_fixture->source_view()));
                require(
                    expected.back() == ranked->rank(time, rank_fixture->source_view()),
                    "Rank depends on the number of rank queries"
                );
            }

            for (auto time : timestamps) {
                (void)(*rank_fixture)(time);
            }

            for (std::size_t i = 0; i < timestamps.size(); ++i) {
                require(
                    expected[i] == ranked->rank(timestamps[i], rank_fixture->source_view()),
                    "Rank changed after trajectory queries at fixed source data"
                );
            }
        }

        auto live = make_fixture();

        for (std::size_t change_index = 0; change_index < changes.size(); ++change_index) {
            for (auto time : timestamps) {
                (void)(*live)(time);
            }

            auto old = make_fixture();

            for (std::size_t i = 0; i < change_index; ++i) {
                old->change(changes[i]);
            }

            auto region =
                old->evaluator().invalidation_region(old->source_view(), changes[change_index]);
            live->change(changes[change_index]);

            for (auto time : timestamps) {
                auto fresh = make_fixture();

                for (std::size_t i = 0; i <= change_index; ++i) {
                    fresh->change(changes[i]);
                }

                const Y expected = (*fresh)(time);

                if (!(*region)(time)) {
                    const Y before = (*old)(time);
                    require(
                        std::invoke(equivalent, before, expected),
                        "Invalidation predicate omitted an affected time"
                    );
                }

                const Y actual = (*live)(time);
                require(
                    std::invoke(equivalent, actual, expected),
                    "Warm trajectory returned a stale result after source update"
                );

                if (auto prescribed = live->source_view().find(time)) {
                    require(
                        std::invoke(equivalent, actual, *prescribed),
                        "Trajectory ignored a prescribed value"
                    );
                }
            }
        }
    }

    /*
        До/после factory задают одинаковый D и разные E. После замены нельзя
        сохранять результаты старого E. Внешний интерфейс траектории остаётся тем
        же, заданные значения сохраняются. Проверка не требует рангов у обоих E.
    */
    template<
        class Y,
        class BeforeFactory,
        class AfterFactory,
        class EvaluatorFactory,
        class Equivalent = std::equal_to<>>
    void test_evaluator_replacement_contract(
        BeforeFactory before,
        AfterFactory after,
        EvaluatorFactory replacement,
        std::span<const simulation::Timestamp> timestamps,
        Equivalent equivalent = {})
    {
        auto live = before();

        for (auto time : timestamps) {
            (void)(*live)(time);
        }

        live->replace_evaluator(replacement());

        for (auto time : timestamps) {
            auto fresh = after();
            const Y actual = (*live)(time);
            const Y expected = (*fresh)(time);
            require(
                std::invoke(equivalent, actual, expected),
                "Evaluator replacement retained an old result"
            );
        }
    }
}
