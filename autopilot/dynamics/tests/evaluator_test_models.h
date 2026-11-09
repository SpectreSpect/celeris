#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "../trajectory_evaluator_interface.h"
#include "component_test_models.h"

namespace celeris::tests::models {
    using namespace std::chrono_literals;

    inline std::unique_ptr<InvalidationRegionInterface> all_times() {
        return std::make_unique<IntervalInvalidationRegion>(
            std::vector<TimeInterval>{TimeInterval{}}
        );
    }

    /*
        Тестовая рекурсивная функция, не численный интегратор:
        t<=0: initial; 0<t<=1s: f(0)+offset; t>1s: f(1s)+offset.
        Все явно заданные значения обслуживает координатор. Зависимости имеют
        ранги 0,1,2. Насыщение делает арифметику определённой для любого int/T.
    */
    class StepEvaluator : public RankedTrajectoryEvaluatorInterface<int> {
    public:
        explicit StepEvaluator(int initial = 10, int offset = 2)
            :   m_initial(initial),
                m_offset(offset) {}

    private:
        int evaluate_impl(Timestamp time, TrajectoryEvaluationContext<int>& context) override {
            if (time <= Timestamp{}) {
                return m_initial;
            }

            const auto dependency = time <= Timestamp{1s} ? Timestamp{} : Timestamp{1s};
            const auto sum = static_cast<std::int64_t>(context.resolve(dependency)) + m_offset;

            return static_cast<int>(
                std::clamp(
                    sum,
                    static_cast<std::int64_t>(std::numeric_limits<int>::min()),
                    static_cast<std::int64_t>(std::numeric_limits<int>::max())
                )
            );
        }

        Rank rank_impl(Timestamp time, const PrescribedValuesViewInterface<int>&) const override {
            return time <= Timestamp{} ?
                0 :
                (time <= Timestamp{1s} ? 1 : 2);
        }

        std::unique_ptr<InvalidationRegionInterface> invalidation_region_impl(
            const PrescribedValuesViewInterface<int>&,
            const PrescribedValuesChange<int>& change) const override
        {
            const auto time = std::visit([](const auto& op) { return op.timestamp; }, change);
            return std::make_unique<IntervalInvalidationRegion>(
                std::vector<TimeInterval>{{time, std::nullopt}}
            );
        }

        int m_initial;
        int m_offset;
    };

    // Та же корректная функция без заявления дополнительного интерфейса рангов.
    class PlainStepEvaluator final : public TrajectoryEvaluatorInterface<int> {
    private:
        int evaluate_impl(Timestamp time, TrajectoryEvaluationContext<int>& context) override {
            return m_inner.evaluate(time, context);
        }

        std::unique_ptr<InvalidationRegionInterface> invalidation_region_impl(
            const PrescribedValuesViewInterface<int>& values,
            const PrescribedValuesChange<int>& change) const override
        {
            return m_inner.invalidation_region(values, change);
        }

        StepEvaluator m_inner;
    };

    class FlatEvaluator : public TrajectoryEvaluatorInterface<int> {
    public:
        explicit FlatEvaluator(int value = 200)
            :   m_value(value) {}

    private:
        int evaluate_impl(Timestamp, TrajectoryEvaluationContext<int>&) override {
            return m_value;
        }

        std::unique_ptr<InvalidationRegionInterface> invalidation_region_impl(
            const PrescribedValuesViewInterface<int>&,
            const PrescribedValuesChange<int>&) const override
        {
            return all_times();
        }

        int m_value;
    };

    class RequiresOriginEvaluator final : public FlatEvaluator {
    private:
        bool compatible_impl(const PrescribedValuesViewInterface<int>& values) const override {
            return values.find(Timestamp{}).has_value();
        }
    };

    class WrongInvalidationEvaluator final : public StepEvaluator {
    private:
        std::unique_ptr<InvalidationRegionInterface> invalidation_region_impl(
            const PrescribedValuesViewInterface<int>&,
            const PrescribedValuesChange<int>&) const override
        {
            return std::make_unique<IntervalInvalidationRegion>(std::vector<TimeInterval>{});
        }
    };

    class WrongRankEvaluator final : public StepEvaluator {
    private:
        Rank rank_impl(Timestamp, const PrescribedValuesViewInterface<int>&) const override {
            return 0;
        }
    };

    class FirstQueryEvaluator final : public FlatEvaluator {
    private:
        int evaluate_impl(Timestamp time, TrajectoryEvaluationContext<int>&) override {
            if (!m_first) {
                m_first = time > Timestamp{} ? 1 : 0;
            }

            return *m_first;
        }

        std::optional<int> m_first;
    };

    class CyclicEvaluator final : public FlatEvaluator {
    private:
        int evaluate_impl(Timestamp time, TrajectoryEvaluationContext<int>& context) override {
            return context.resolve(time);
        }
    };

    class NullRegionEvaluator final : public FlatEvaluator {
    private:
        std::unique_ptr<InvalidationRegionInterface> invalidation_region_impl(
            const PrescribedValuesViewInterface<int>&,
            const PrescribedValuesChange<int>&) const override
        {
            return nullptr;
        }
    };

    class ThrowOnceEvaluator final : public FlatEvaluator {
    private:
        int evaluate_impl(Timestamp time, TrajectoryEvaluationContext<int>& context) override {
            if (time > Timestamp{1s} && m_fail) {
                (void)context.resolve(Timestamp{1s});
                m_fail = false;
                throw InjectedFailure();
            }

            return m_inner.evaluate(time, context);
        }

        bool m_fail = true;
        StepEvaluator m_inner;
    };

    /*
        Проверяет разрешённое заимствование старого D: при преждевременной записи
        в источник предикат перестал бы видеть изменение origin и пропустил кэш.
    */
    class BorrowingRegionEvaluator final : public StepEvaluator {
    private:
        std::unique_ptr<InvalidationRegionInterface> invalidation_region_impl(
            const PrescribedValuesViewInterface<int>& values,
            const PrescribedValuesChange<int>& change) const override
        {
            auto predicate = [&values, &change](Timestamp) {
                return std::visit(
                    [&](const auto& operation) {
                        if (operation.timestamp != Timestamp{}) {
                            return true;
                        }

                        const auto old = values.find(Timestamp{});

                        if constexpr (requires { operation.value; }) {
                            return !old || *old != operation.value;
                        } else {
                            return old.has_value();
                        }
                    },
                    change
                );
            };

            return std::make_unique<PredicateInvalidationRegion<decltype(predicate)>>(predicate);
        }
    };

    class ThrowingRegionEvaluator final : public StepEvaluator {
    private:
        std::unique_ptr<InvalidationRegionInterface> invalidation_region_impl(
            const PrescribedValuesViewInterface<int>&,
            const PrescribedValuesChange<int>&) const override
        {
            if (!*m_fail) {
                return all_times();
            }

            auto predicate = [fail = m_fail, calls = 0](Timestamp) mutable {
                if (++calls == 2) {
                    *fail = false;
                    throw InjectedFailure();
                }

                return true;
            };

            /*
                Предикат интерфейса вызывается как const. Счётчик технического
                сбоя размещаем в общем состоянии, не меняющем bool-ответы.
            */
            auto state = std::make_shared<decltype(predicate)>(std::move(predicate));
            auto callable = [state](Timestamp time) { return (*state)(time); };
            return std::make_unique<PredicateInvalidationRegion<decltype(callable)>>(callable);
        }

        std::shared_ptr<bool> m_fail = std::make_shared<bool>(true);
    };

    class FailCommitSource : public MapSource<int> {
    public:
        explicit FailCommitSource(std::shared_ptr<bool> fail)
            :   m_fail(std::move(fail)) {}

        void set(Timestamp time, int value) {
            if (*m_fail) {
                *m_fail = false;
                throw InjectedFailure();
            }

            MapSource<int>::set(time, value);
        }

    private:
        std::shared_ptr<bool> m_fail;
    };
}
